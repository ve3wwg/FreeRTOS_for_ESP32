// tasknfy5.ino

// LED GPIOs:
#define GPIO_LED1     18
#define GPIO_LED2     19
#define GPIO_LED3     21

// Button GPIOs:
#define GPIO_BUT1     27
#define GPIO_BUT2     26
#define GPIO_BUT3     25

#define N_BUTTONS     3

// ISR Routines, forward decls:
static void IRAM_ATTR isr_gpio1();
static void IRAM_ATTR isr_gpio2();
static void IRAM_ATTR isr_gpio3();

typedef void (*isr_t)();    // ISR routine type

static struct s_button {
  int           butn_gpio;  // Button
  int           led_gpio;   // LED
  unsigned      buttonx;    // Button index
  isr_t         isr;        // ISR routine
} buttons[N_BUTTONS] = {
  { GPIO_BUT1, GPIO_LED1, 0u, isr_gpio1 },
  { GPIO_BUT2, GPIO_LED2, 1u, isr_gpio2 },
  { GPIO_BUT3, GPIO_LED3, 2u, isr_gpio3 }
};

static TaskHandle_t htask1;

// Task1 for sensing buttons

static void task1(void *arg) {
  uint32_t rv;
  BaseType_t rc;

  for (;;) {
    rc = xTaskNotifyWait(0,0b0111,&rv,portMAX_DELAY);
    printf("Task notified: rv=%u\n",unsigned(rv));
    for ( unsigned x=0; x<3; ++x ) {
      if ( rv & (1 << x) )
        printf("  Button %u notified, reads %d\n",
          x,digitalRead(buttons[x].butn_gpio));
        digitalWrite(buttons[x].led_gpio,
          digitalRead(buttons[x].butn_gpio));
    }
  }
}

// Generalized ISR for each GPIO

inline static BaseType_t IRAM_ATTR isr_gpiox(uint8_t gpiox) {
  s_button& button = buttons[gpiox];
  BaseType_t woken = pdFALSE;
  
  xTaskNotifyFromISR(htask1,1 << button.buttonx,eSetBits,&woken);
  return woken;
}  

// ISR specific to Button 1

static void IRAM_ATTR isr_gpio1() {
  if ( isr_gpiox(0) )
    portYIELD_FROM_ISR();
}

// ISR specific to Button 2

static void IRAM_ATTR isr_gpio2() {
  if ( isr_gpiox(1) )
    portYIELD_FROM_ISR();
}

// ISR specific to Button 3

static void IRAM_ATTR isr_gpio3() {
  if ( isr_gpiox(2) )
    portYIELD_FROM_ISR();
}

// Initialization

void setup() {
  int app_cpu = xPortGetCoreID();
  BaseType_t rc;

  // For each button + LED pair:
  for ( unsigned ux=0; ux<N_BUTTONS; ++ux ) {
    s_button& button = buttons[ux];

    pinMode(button.led_gpio,OUTPUT);
    digitalWrite(button.led_gpio,1);
    pinMode(button.butn_gpio,INPUT_PULLUP);
    attachInterrupt(button.butn_gpio,button.isr,CHANGE);
  }

  delay(2000); // Allow USB to connect
  printf("tasknfy5.ino:\n");

  rc = xTaskCreatePinnedToCore(
    task1,    // Task function
    "task1",  // Name
    3000,     // Stack size
    nullptr,  // Parameters
    1,        // Priority
    &htask1,  // handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);
}

// Not used:

void loop() {
  vTaskDelete(nullptr);
}
