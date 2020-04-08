// debounce.ino 
// MIT License (see file LICENSE)

// LED is active high
#define GPIO_LED    12 
#define GPIO_BUTTON 25

//
// Button Debouncing task:
//
static void IRAM_ATTR my_isr() {
  static uint32_t level, state = 0, last = 0xFFFFFFFF;
  static const uint32_t mask = 0x7FFFFFFF;
  static bool led = false;
  bool event;
  
  level = !!digitalRead(GPIO_BUTTON);
  state = (state << 1) | level;
  if ( (state & mask) == mask
    || (state & mask) == 0 ) {
    if ( level != last ) {
      event = !!level;
      led ^= true;
      digitalWrite(GPIO_LED,led);
    }
  }
}

//
// Initialization:
//
void setup() {

  pinMode(GPIO_LED,OUTPUT);
  pinMode(GPIO_BUTTON,INPUT_PULLUP);
  attachInterrupt(GPIO_BUTTON,my_isr,CHANGE);
}

// Not used:
void loop() {
  vTaskDelete(nullptr);
}
