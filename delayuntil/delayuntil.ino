// delayuntil.ino 
// MIT License (see file LICENSE)

// LED is active high
#define GPIO_LED1     12 
#define GPIO_LED2     13

static volatile bool startf = false;
static TickType_t period = 250;

static void big_think() {

  for ( int x=0; x<40000; ++x )
    __asm__ __volatile__ ("nop");
}

static void task1(void *argp) {
  bool state = true;

  while ( !startf )
    ;

  for (;;) {
    state ^= true;
    digitalWrite(GPIO_LED1,state);
    big_think();
    delay(period);
  }
}

static void task2(void *argp) {
  bool state = true;

  while ( !startf )
    ;

  TickType_t ticktime = xTaskGetTickCount();

  for (;;) {
    state ^= true;
    digitalWrite(GPIO_LED2,state);
    big_think();
    vTaskDelayUntil(&ticktime,period);
  }
}

//
// Initialization:
//
void setup() {
  int app_cpu = xPortGetCoreID();
  BaseType_t rc;

  // delay(2000); // Allow USB to connect
  pinMode(GPIO_LED1,OUTPUT);
  pinMode(GPIO_LED2,OUTPUT);
  digitalWrite(GPIO_LED1,HIGH);
  digitalWrite(GPIO_LED2,HIGH);

  rc = xTaskCreatePinnedToCore(
    task1,
    "task1",
    2048,
    nullptr,
    1,
    nullptr,
    app_cpu);
  assert(rc == pdPASS);

  rc = xTaskCreatePinnedToCore(
    task2,
    "task2",
    2048,
    nullptr,
    1,
    nullptr,
    app_cpu);
  assert(rc == pdPASS);

  startf = true;
}

void loop() {
  delay(50);
}
