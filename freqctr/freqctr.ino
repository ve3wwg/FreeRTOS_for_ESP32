// freqctr.ino - Wemos Lolin ESP32

#define GPIO_PULSEIN  25
#define GPIO_FREQGEN  26
#define GPIO_ADC      14

// GPIO for PWM output
#define PWM_GPIO      GPIO_FREQGEN
#define PWM_CH        0
#define PWM_FREQ      2000
#define PWM_RES       1

#include "SSD1306.h"
#include "driver/periph_ctrl.h"
#include "driver/pcnt.h"

#define SSD1306_ADDR  0x3C
#define SSD1306_SDA   5
#define SSD1306_SCL   4

static SSD1306 oled(
  SSD1306_ADDR,
  SSD1306_SDA,
  SSD1306_SCL
);

static int app_cpu = 0;
static pcnt_isr_handle_t isr_handle = nullptr;
static SemaphoreHandle_t sem_h;
static QueueHandle_t evtq_h;

static void IRAM_ATTR pulse_isr(void *arg) {
  static uint32_t usecs0;
  uint32_t intr_status = PCNT.int_st.val;
  uint32_t evt_status, usecs;
  BaseType_t woken = pdFALSE;
  
  if ( intr_status & BIT(0) ) {
    // PCNT_UNIT_0 Interrupt
    evt_status = PCNT.status_unit[0].val;
    if ( evt_status & PCNT_STATUS_THRES0_M ) {
      usecs0 = micros();
    } else if ( evt_status & PCNT_STATUS_THRES1_M ) {
      usecs = micros() - usecs0;
      xQueueSendFromISR(evtq_h,&usecs,&woken);
      pcnt_counter_pause(PCNT_UNIT_0);
    }
    PCNT.int_clr.val = BIT(0);
  }
  if ( woken ) {
    portYIELD_FROM_ISR();
  }
}

static void lock_oled() {
  xSemaphoreTake(sem_h,portMAX_DELAY);
}

static void unlock_oled() {
  xSemaphoreGive(sem_h);
}

static void oled_gen(uint32_t f) {
  char buf[32];

  snprintf(buf,sizeof buf,"%u gen",f);
  lock_oled();
  oled.setColor(BLACK);
  oled.fillRect(0,0,128,31);
  oled.setColor(WHITE);
  oled.drawString(64,0,buf);
  oled.drawHorizontalLine(0,26,128);
  oled.display();
  unlock_oled();
}

static void oled_freq(uint32_t f) {
  char buf[32];

  snprintf(buf,sizeof buf,"%u Hz",f);
  lock_oled();
  oled.setColor(BLACK);
  oled.fillRect(0,32,128,63);
  oled.setColor(WHITE);
  oled.drawString(64,32,buf);
  oled.display();
  unlock_oled();
}

static uint32_t retarget(uint32_t freq,uint32_t usec) {
  static const uint32_t target_usecs = 100000;
  uint64_t f = freq, t, u;

  auto target = [&freq](uint32_t usec) {
    if ( freq > 100000 )
      return uint64_t(freq) / 1000 * usec / 1000 + 10;
    else
      return uint64_t(freq) * usec / 1000000 + 10;
  };

  auto useconds = [&freq](uint64_t t) {
    return (t - 10) * 1000000 / freq;
  };

  if ( (t = target(usec)) > 32000 ) {
    t = target(target_usecs);
    if ( t > 32500 )
      t = 32500;
    u = useconds(t);
    t = target(u);
  } else {
    t = target(target_usecs);
    if ( t < 25 )
      t = 25;
    u = useconds(t);
    t = target(u);
  }
  if ( t > 32500 )
    t = 32500;
  else if ( t < 25 )
    t = 25;
  return t;
}

static void monitor(void *arg) {
  uint32_t usecs;
  int16_t thres;
  BaseType_t rc;

  for (;;) {
    rc = pcnt_counter_clear(PCNT_UNIT_0);
    assert(!rc);
    xQueueReset(evtq_h);
    rc = pcnt_counter_resume(PCNT_UNIT_0);
    assert(!rc);

    rc = pcnt_get_event_value(PCNT_UNIT_0,PCNT_EVT_THRES_1,&thres);
    assert(!rc);
    rc = xQueueReceive(evtq_h,&usecs,500);
    if ( rc == pdPASS ) {
      uint32_t freq = uint64_t(thres-10)
          * uint64_t(1000000) / usecs;
      oled_freq(freq);
      thres = retarget(freq,usecs);
      rc = pcnt_set_event_value(PCNT_UNIT_0,PCNT_EVT_THRES_1,thres);
      assert(!rc);
    } else {
      rc = pcnt_counter_pause(PCNT_UNIT_0);
      assert(!rc);
      rc = pcnt_set_event_value(PCNT_UNIT_0,PCNT_EVT_THRES_1,25);
      assert(!rc);
    }
  }
}

static void oled_init() {
  oled.init();
  oled.clear();
  oled.flipScreenVertically();
  oled.invertDisplay();
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.setFont(ArialMT_Plain_24);
  oled.drawString(64,0,"freqctr.ino");
  oled.drawHorizontalLine(0,0,128);
  oled.drawHorizontalLine(0,26,128);
  oled.display();
}

static void analog_init() {
  adcAttachPin(GPIO_ADC);
  analogReadResolution(12);
  analogSetPinAttenuation(GPIO_ADC,ADC_11db);
}

static void counter_init() {
  pcnt_config_t cfg;
  int rc;
  
  memset(&cfg,0,sizeof cfg);
  cfg.pulse_gpio_num = GPIO_PULSEIN;
  cfg.ctrl_gpio_num = PCNT_PIN_NOT_USED;
  cfg.channel = PCNT_CHANNEL_0;
  cfg.unit = PCNT_UNIT_0;
  cfg.pos_mode = PCNT_COUNT_INC; // Count up on the positive edge
  cfg.neg_mode = PCNT_COUNT_DIS;
  cfg.lctrl_mode = PCNT_MODE_KEEP;
  cfg.hctrl_mode = PCNT_MODE_KEEP;
  cfg.counter_h_lim = 32767;
  cfg.counter_l_lim = 0;
  rc = pcnt_unit_config(&cfg);
  assert(!rc);
  
  rc = pcnt_set_event_value(PCNT_UNIT_0,PCNT_EVT_THRES_0,10);
  assert(!rc);
  rc = pcnt_set_event_value(PCNT_UNIT_0,PCNT_EVT_THRES_1,10000);
  assert(!rc);
  rc = pcnt_event_enable(PCNT_UNIT_0,PCNT_EVT_THRES_0);
  assert(!rc);
  rc = pcnt_event_enable(PCNT_UNIT_0,PCNT_EVT_THRES_1);
  assert(!rc);
  rc = pcnt_counter_pause(PCNT_UNIT_0);
  assert(!rc);
  rc = pcnt_isr_register(pulse_isr,nullptr,0,&isr_handle);
  assert(!rc);
  rc = pcnt_intr_enable(PCNT_UNIT_0);
  assert(!rc);
}

void setup() {
  unsigned ms;
  BaseType_t rc;  // Return code

  app_cpu = xPortGetCoreID();
  sem_h = xSemaphoreCreateMutex();
  assert(sem_h);
  evtq_h = xQueueCreate(20,sizeof(uint32_t));
  assert(evtq_h);

  // Use PWM to drive CLKIN
  ledcSetup(PWM_CH,PWM_FREQ,PWM_RES);
  ledcAttachPin(PWM_GPIO,PWM_CH);
  ledcWrite(PWM_CH,1); // 50%

  counter_init();
  oled_init();
  delay(2000);

  // Start the monitor task
  rc = xTaskCreatePinnedToCore(
    monitor,    // Function
    "monitor",  // Name
    4096,       // Stack size
    nullptr,    // Argument
    1,          // Priority
    nullptr,    // Handle ptr
    app_cpu     // CPU
  );
  assert(rc == pdPASS);
}

void loop() {
  uint32_t f; // Frequency

  f = analogRead(GPIO_ADC) * 80 + 500;
  oled_gen(f);

  ledcSetup(PWM_CH,f,PWM_RES);
  ledcAttachPin(PWM_GPIO,PWM_CH);
  ledcWrite(PWM_CH,1); // 50%
  delay(500);
}

