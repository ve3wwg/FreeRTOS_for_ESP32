// hcsr04.ino 
// MIT License (see file LICENSE)

// Set to zero if not using OLED
#define USE_SSD1306   1

// GPIO definitions:
// LED is active high
#define GPIO_LED      12 
#define GPIO_TRIGGER  25
#define GPIO_ECHO     26

#if USE_SSD1306
#include "SSD1306.h"

#define SSD1306_ADDR  0x3C
#define SSD1306_SDA   5
#define SSD1306_SCL   4

static SSD1306 oled(
  SSD1306_ADDR,
  SSD1306_SDA,
  SSD1306_SCL
);
#endif

typedef unsigned long usec_t;

static SemaphoreHandle_t barrier;
static TickType_t repeat_ticks = 1000;

//
// Report the distance in CM
//
static void report_cm(usec_t usecs) {
  unsigned cm, tenths;

  cm = usecs * 10ul / 58ul;
  tenths = cm % 10;
  cm /= 10;

  printf("Distance %u.%u cm, %u usecs\n",
    cm,tenths,usecs);

#if USE_SSD1306
  {
    char buf[40];

    snprintf(buf,sizeof buf,
      "%u.%u cm",
      cm,tenths);
    oled.setColor(BLACK);
    oled.fillRect(0,27,128,64);
    oled.setColor(WHITE);
    oled.drawString(64,35,buf);
    oled.display();
  }
#endif
}

//
// Range Finder Task:
//
static void range_task(void *argp) {
  BaseType_t rc;
  usec_t usecs;

  for (;;) {
    rc = xSemaphoreTake(barrier,portMAX_DELAY);
    assert(rc == pdPASS);

    // Send ping:
    digitalWrite(GPIO_LED,HIGH);
    digitalWrite(GPIO_TRIGGER,HIGH);
    delayMicroseconds(10);
    digitalWrite(GPIO_TRIGGER,LOW);

    // Listen for echo:
    usecs = pulseInLong(GPIO_ECHO,HIGH,50000);
    digitalWrite(GPIO_LED,LOW);

    if ( usecs > 0 && usecs < 50000UL )
      report_cm(usecs);
    else
      printf("No echo\n");
  }
}

//
// Send sync to range_task every 1 sec
//
static void sync_task(void *argp) {
  BaseType_t rc;
  TickType_t ticktime;

  delay(1000);

  ticktime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&ticktime,repeat_ticks);
    rc = xSemaphoreGive(barrier);
    // assert(rc == pdPASS);
  }
}

//
// Program Initialization
//
void setup() {
  int app_cpu = xPortGetCoreID();
  TaskHandle_t h;
  BaseType_t rc;

  barrier = xSemaphoreCreateBinary();
  assert(barrier);

  pinMode(GPIO_LED,OUTPUT);
  digitalWrite(GPIO_LED,LOW);
  pinMode(GPIO_TRIGGER,OUTPUT);
  digitalWrite(GPIO_TRIGGER,LOW);
  pinMode(GPIO_ECHO,INPUT_PULLUP);

#if USE_SSD1306
  oled.init();
  oled.clear();
  oled.flipScreenVertically();
  oled.setColor(WHITE);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.setFont(ArialMT_Plain_24);
  oled.drawString(64,0,"hcsr04.ino");
  oled.drawHorizontalLine(0,0,128);
  oled.drawHorizontalLine(0,26,128);
  oled.display();
#endif

  delay(2000); // Allow USB to connect

  printf("\nhcsr04.ino:\n");

  rc = xTaskCreatePinnedToCore(
    range_task,
    "rangetsk",
    2048,     // Stack size
    nullptr,
    1,        // Priority
    &h,       // Task handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);
  assert(h);

  rc = xTaskCreatePinnedToCore(
    sync_task,
    "synctsk",
    2048,     // Stack size
    nullptr,
    1,        // Priority
    &h,       // Task handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);
  assert(h);
}

// Not used:
void loop() {
  vTaskDelete(nullptr);
}
