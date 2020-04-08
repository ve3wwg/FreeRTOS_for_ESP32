// freqctr.ino - TTGO ESP32 T-Display

#define GPIO_PULSEIN  25
#define GPIO_FREQGEN  26
#define GPIO_ADC      15

// GPIO for PWM output
#define PWM_GPIO      GPIO_FREQGEN
#define PWM_CH        0
#define PWM_FREQ      2000
#define PWM_RES       1

#include <SPI.h>
#include <TFT_eSPI.h>
#include "driver/periph_ctrl.h"
#include "driver/pcnt.h"

static TFT_eSPI tft;

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

static void lock_tft() {
  xSemaphoreTake(sem_h,portMAX_DELAY);
}

static void unlock_tft() {
  xSemaphoreGive(sem_h);
}

static void tft_gen(uint32_t f) {
  char buf[32];
  const char *cp;

  snprintf(buf,sizeof buf,"%u Hz",f);

  lock_tft();
  int16_t height = tft.height() / 2;
  int16_t width = tft.width();
  int16_t fheight = tft.fontHeight(4);
  tft.fillRect(0,0,width,height,TFT_GREEN);
  tft.setTextColor(TFT_BLACK);
  tft.setTextFont(4);
  cp = "PWM Generator:";
  tft.drawString(cp,
    (width-tft.textWidth(cp))/2,
    (height/2-fheight));
  tft.drawString(buf,
    (width-tft.textWidth(buf))/2,
    (height/2));
  unlock_tft();
}

static void tft_freq(uint32_t f) {
  char buf[32];
  const char *cp;

  snprintf(buf,sizeof buf,"%u Hz",f);
  lock_tft();
  int16_t height = tft.height() / 2;
  int16_t width = tft.width();
  int16_t fheight = tft.fontHeight(4);
  tft.fillRect(0,height,width,tft.height(),TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(4);
  cp = "Freq. Counter:";
  tft.drawString(cp,
    (width-tft.textWidth(cp))/2,
    height+height/2-fheight+5);
  tft.drawString(buf,
    (width-tft.textWidth(buf))/2,
    height+height/2+5);
  unlock_tft();
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
      tft_freq(freq);
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

static void tft_init() {
  const char *cp;

  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(TFT_GREEN);
  tft.setTextColor(TFT_BLACK);
  tft.setTextFont(4);
  cp = "freqctr-ttgo.ino";
  tft.drawString(cp,
    (tft.width()-tft.textWidth(cp))/2,
    (tft.height()-tft.fontHeight(4))/2);
  delay(2000);
  tft.fillScreen(TFT_RED);
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
  tft_init();

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
  tft_gen(f);

  ledcSetup(PWM_CH,f,PWM_RES);
  ledcAttachPin(PWM_GPIO,PWM_CH);
  ledcWrite(PWM_CH,1); // 50%
  delay(500);
}

