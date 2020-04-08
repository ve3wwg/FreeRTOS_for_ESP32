// taskcreate.ino 
// MIT License (see file LICENSE)

static int app_cpu = 0;

void task2(void *argp) {

  printf("Task2 executing, priority %u.\n",
    (unsigned)uxTaskPriorityGet(nullptr));
  vTaskDelete(nullptr);
}

void task1(void *argp) {
  BaseType_t rc;
  TaskHandle_t h2;

  printf("Task1 executing, priority %u.\n",
    (unsigned)uxTaskPriorityGet(nullptr));

  rc = xTaskCreatePinnedToCore(
    task2,
    "task2",
    2000,     // Stack size
    nullptr,
    4,        // Priority
    nullptr,  // Task handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);
  printf("Task2 created.\n");
  vTaskDelete(nullptr);
}

void setup() {
  BaseType_t rc;
  unsigned priority = 0;
  TaskHandle_t h1;

  app_cpu = xPortGetCoreID();

  delay(2000); // Allow USB init time

  vTaskPrioritySet(nullptr,3);
  priority = uxTaskPriorityGet(nullptr);
  assert(priority == 3);

  printf("\ntaskcreate.ino:\n");
  printf("loopTask priority is %u.\n",
    priority);

  rc = xTaskCreatePinnedToCore(
    task1,
    "task1",
    2000,     // Stack size
    nullptr,
    2,        // Priority
    &h1,      // Task handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);
  // delay(1);
  printf("Task1 created.\n");

  vTaskPrioritySet(h1,3);
}

// Not used:
void loop() {
  vTaskDelete(nullptr);
}
