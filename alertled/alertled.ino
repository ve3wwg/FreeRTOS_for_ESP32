// alertled.ino 
// MIT License (see file LICENSE)

// LED is active high
#define GPIO_LED      12 

//
// AlertLED class to drive LED
//
class AlertLED {
  TimerHandle_t     thandle = nullptr;
  volatile bool     state;
  volatile unsigned count;
  unsigned          period_ms;
  int               gpio;

  void reset(bool s);

public:
  AlertLED(int gpio,unsigned period_ms=1000);
  void alert();
  void cancel();

  static void callback(TimerHandle_t th);
};

//
// Constructor:
//  gpio        GPIO pin to drive LED on
//  period_ms   Overall period in ms
//
AlertLED::AlertLED(int gpio,unsigned period_ms) {
  this->gpio = gpio;
  this->period_ms = period_ms;
  pinMode(this->gpio,OUTPUT);  
  digitalWrite(this->gpio,LOW);
}

//
// Internal method to reset values
//
void AlertLED::reset(bool s) {
  state = s;
  count = 0;
  digitalWrite(this->gpio,s?HIGH:LOW);
}  

//
// Method to start the alert:
//
void AlertLED::alert() {

  if ( !thandle ) {
    thandle = xTimerCreate(
      "alert_tmr",
      pdMS_TO_TICKS(period_ms/20),
      pdTRUE,
      this,
      AlertLED::callback);
    assert(thandle);
  }
  reset(true);
  xTimerStart(thandle,portMAX_DELAY);
}

//
// Method to stop an alert:
//
void AlertLED::cancel() {
  if ( thandle ) {
    xTimerStop(thandle,portMAX_DELAY);
    digitalWrite(gpio,LOW);
  }
}

// static method, acting as the
// timer callback:
//
void AlertLED::callback(TimerHandle_t th) {
  AlertLED *obj = (AlertLED*)pvTimerGetTimerID(th);

  assert(obj->thandle == th);
  obj->state ^= true;
  digitalWrite(obj->gpio,obj->state?HIGH:LOW);

  if ( ++obj->count >= 5 * 2 ) {
    obj->reset(true);
    xTimerChangePeriod(th,pdMS_TO_TICKS(obj->period_ms/20),portMAX_DELAY);
  } else if ( obj->count == 5 * 2 - 1 ) {
    xTimerChangePeriod(th,
      pdMS_TO_TICKS(obj->period_ms/20+obj->period_ms/2),
      portMAX_DELAY);
    assert(!obj->state);
  }
}

//
// Global objects
//
static AlertLED alert1(GPIO_LED,1000);
static unsigned loop_count = 0;

//
// Initialization:
//
void setup() {
  // delay(2000); // Allow USB to connect
  alert1.alert();
}

void loop() {
  if ( loop_count >= 70 ) {
    alert1.alert();
    loop_count = 0;
  }

  delay(100);

  if ( ++loop_count >= 50 )
    alert1.cancel();
}
