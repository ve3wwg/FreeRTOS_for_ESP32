// ticks.ino 
// MIT License (see file LICENSE)

#define GPIO  12

static void gpio_on(void *argp) {
  for (;;) {
    digitalWrite(GPIO,HIGH);
  }
}

static void gpio_off(void *argp) {
  for (;;) {
    digitalWrite(GPIO,LOW);
  }
}

void setup() {
  int app_cpu = xPortGetCoreID();

  pinMode(GPIO,OUTPUT);
  delay(1000);
  printf("Setup started..\n");

  xTaskCreatePinnedToCore(
    gpio_on,
    "gpio_on",
    2048,
    nullptr,
    1,
    nullptr,
    app_cpu
  );
  xTaskCreatePinnedToCore(
    gpio_off,
    "gpio_off",
    2048,
    nullptr,
    1,
    nullptr,
    app_cpu
  );
}

void loop() {
  vTaskDelete(xTaskGetCurrentTaskHandle());
}
