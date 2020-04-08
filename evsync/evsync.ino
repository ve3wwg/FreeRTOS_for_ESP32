// evsync.ino

// LED GPIOs:
#define GPIO_LED1     18
#define GPIO_LED2     19
#define GPIO_LED3     21

#define N_LED         3

#define EV_RDY        0b1000
#define EV_ALL        (EV_RDY|0b0111)

static EventGroupHandle_t hevt;
static int leds[N_LED] =
  { GPIO_LED1, GPIO_LED2, GPIO_LED3 };

static void led_task(void *arg) {
  unsigned ledx = (unsigned)arg;  // LED Index
  EventBits_t our_ev = 1 << ledx; // Our event
  EventBits_t rev;
  TickType_t timeout;
  unsigned seed = ledx;

  assert(ledx < N_LED);

  for (;;) {
    timeout = rand_r(&seed) % 100 + 10;
    rev = xEventGroupSync(
      hevt,       // Group event
      our_ev,     // Our bit to set
      EV_ALL,     // All bits required
      timeout);   // Timeout

    if ( (rev & EV_ALL) == EV_ALL ) {
      // Not timed out: blink LED
      digitalWrite(leds[ledx],
        !digitalRead(leds[ledx]));
    }
  }
}

void setup() {
  int app_cpu = xPortGetCoreID();
  BaseType_t rc;

  // Create Event Group
  hevt = xEventGroupCreate();
  assert(hevt);

  // Configure LED GPIOs
  for ( int x=0; x<N_LED; ++x ) {
    pinMode(leds[x],OUTPUT);
    digitalWrite(leds[x],LOW);

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
  delay(1000);
  xEventGroupSetBits(hevt,EV_RDY);
}
