// critical.ino

// LED GPIOs:
#define GPIO_LED1     18
#define GPIO_LED2     19
#define GPIO_LED3     21

void setup() {
  // Configure LED GPIOs
  pinMode(GPIO_LED1,OUTPUT);
  pinMode(GPIO_LED2,OUTPUT);
  pinMode(GPIO_LED3,OUTPUT);
  digitalWrite(GPIO_LED1,LOW);
  digitalWrite(GPIO_LED2,LOW);
  digitalWrite(GPIO_LED3,LOW);
}

void loop() {
  static portMUX_TYPE mutex =
    portMUX_INITIALIZER_UNLOCKED;
  bool state = !!digitalRead(GPIO_LED1) ^ 1;

  // Change 3 LEDs at once
  portENTER_CRITICAL(&mutex);
  digitalWrite(GPIO_LED1,state);
  digitalWrite(GPIO_LED2,state);
  digitalWrite(GPIO_LED3,state);
  portEXIT_CRITICAL(&mutex);

  delay(500);
}
