// tasknfy4.ino

#define GPIO_LED        12

static TaskHandle_t htask1;

static void task1(void *arg) {
  uint32_t rv;
  BaseType_t rc;

  for (;;) {
    rc = xTaskNotifyWait(0,0b0011,&rv,portMAX_DELAY);
    digitalWrite(GPIO_LED,digitalRead(GPIO_LED)^HIGH);
    printf("Task notified: rv=%u\n",unsigned(rv));
    if ( rv & 0b0001 )
      printf("  loop() notified this task.\n");
    if ( rv & 0b0010 )
      printf("  task2() notified this task.\n");
  }
}

static void task2(void *arg) {
  unsigned count;
  BaseType_t rc;

  for (;; count += 100u ) {
    delay(500+count);
    rc = xTaskNotify(htask1,0b0010,eSetBits);
    assert(rc == pdPASS);
  }
}

void setup() {
  int app_cpu = 0;
  BaseType_t rc;

  app_cpu = xPortGetCoreID();
  pinMode(GPIO_LED,OUTPUT);
  digitalWrite(GPIO_LED,LOW);

  delay(2000); // Allow USB to connect
  printf("tasknfy4.ino:\n");

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

  rc = xTaskCreatePinnedToCore(
    task2,    // Task function
    "task2",  // Name
    3000,     // Stack size
    nullptr,  // Parameters
    1,        // Priority
    nullptr,  // no handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);
}

void loop() {
  BaseType_t rc;

  delay(500);
  rc = xTaskNotify(htask1,0b0001,eSetBits);
  assert(rc == pdPASS);
}
