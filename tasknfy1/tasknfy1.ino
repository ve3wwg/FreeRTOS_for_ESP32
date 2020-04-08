// tasknfy1.ino

#define GPIO_LED        12

static TaskHandle_t htask1;

static void task1(void *arg) {
  uint32_t rv;

  for (;;) {
    rv = ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    digitalWrite(GPIO_LED,digitalRead(GPIO_LED)^HIGH);
    printf("Task notified: rv=%u\n",unsigned(rv));
  }
}

void setup() {
  int app_cpu = 0;
  BaseType_t rc;

  app_cpu = xPortGetCoreID();
  pinMode(GPIO_LED,OUTPUT);
  digitalWrite(GPIO_LED,LOW);

  delay(2000); // Allow USB to connect
  printf("tasknfy1.ino:\n");

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

void loop() {
  delay(1000);
  xTaskNotifyGive(htask1);
}
