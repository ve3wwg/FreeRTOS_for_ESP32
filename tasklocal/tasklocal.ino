// tasklocal.ino

// LED GPIOs:
#define GPIO_LED1   18
#define GPIO_LED2   19
#define GPIO_LED3   21

#define N_LED       3

static int leds[N_LED] =
  { GPIO_LED1, GPIO_LED2, GPIO_LED3 };

struct s_task_local {
  int     index;
  int     led_gpio;
  bool    state;
};

static void blink_led() {
  s_task_local *plocal = (s_task_local*)
    pvTaskGetThreadLocalStoragePointer(nullptr,0);

  delay(plocal->index*250+250);
  plocal->state ^= true;
  digitalWrite(plocal->led_gpio,plocal->state);
}

static void led_task(void *arg) {
  int x = (int)arg;
  s_task_local *plocal = new s_task_local;

  plocal->index = x;
  plocal->led_gpio = leds[x];
  plocal->state = false;

  pinMode(plocal->led_gpio,OUTPUT);
  digitalWrite(plocal->led_gpio,LOW);

  vTaskSetThreadLocalStoragePointer(
    nullptr,
    0,
    plocal);

  for (;;) {
    blink_led();
  }
}

void setup() {
  int app_cpu = xPortGetCoreID();
  BaseType_t rc;

  for ( int x=0; x<N_LED; ++x ) {
    rc = xTaskCreatePinnedToCore(
      led_task, // function
      "ledtsk", // Name
      2100,     // Stack size
      (void*)x, // Parameters
      1,        // Priority
      nullptr,  // handle
      app_cpu   // CPU
    );
    assert(rc == pdPASS);
  }
}

void loop() {
  vTaskDelete(nullptr);
}
