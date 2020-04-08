// stubs.ino

static void init_oled() {
  printf("init_oled() called.\n");
}

static void init_gpio() {
  printf("init_gpio() called.\n");
}

void setup() {
  delay(2000); // Allow for serial setup
  printf("Hello from setup()\n");
  init_oled();
  init_gpio();
}

void loop() {
  printf("Hello from loop()\n");
  delay(1000);
}
