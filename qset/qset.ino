// qset.ino - Demonstrate Queue Set

// LED GPIOs:
#define GPIO_LED1     18
#define GPIO_LED2     19
#define GPIO_LED3     21

// Button GPIOs:
#define GPIO_BUT1     27
#define GPIO_BUT2     26
#define GPIO_BUT3     25

#define N_BUTTONS     3
#define Q_DEPTH       8

// ISR Routines, forward decls:
static void IRAM_ATTR isr_gpio1();
static void IRAM_ATTR isr_gpio2();
static void IRAM_ATTR isr_gpio3();

typedef void (*isr_t)();    // ISR routine type

static struct s_button {
  int           butn_gpio;  // Button
  int           led_gpio;   // LED
  QueueHandle_t qh;         // Queue
  isr_t         isr;        // ISR routine
} buttons[N_BUTTONS] = {
  { GPIO_BUT1, GPIO_LED1, nullptr, isr_gpio1 },
  { GPIO_BUT2, GPIO_LED2, nullptr, isr_gpio2 },
  { GPIO_BUT3, GPIO_LED3, nullptr, isr_gpio3 }
};

// Event Task:

static void evtask(void *arg) {
  QueueSetHandle_t hqset = (QueueSetHandle_t*)arg;
  QueueSetMemberHandle_t mh;
  bool bstate;
  BaseType_t rc;

  for (;;) {
    // Wait for an event from our 3 queues:
    mh = xQueueSelectFromSet(hqset,portMAX_DELAY);
    for ( unsigned ux=0; ux<N_BUTTONS; ++ux ) {
      s_button& button = buttons[ux];

      if ( mh == button.qh ) {
        rc = xQueueReceive(mh,&bstate,0);
        assert(rc == pdPASS);
        digitalWrite(button.led_gpio,bstate);          
        break;
      }
    }
  }
}

// Generalized ISR for each GPIO

inline static BaseType_t IRAM_ATTR isr_gpiox(uint8_t gpiox) {
  s_button& button = buttons[gpiox];
  bool state = digitalRead(button.butn_gpio);
  BaseType_t woken = pdFALSE;
  
  (void)xQueueSendToBackFromISR(button.qh,&state,&woken);
  return woken;
}  

// ISR specific to Button 1

static void IRAM_ATTR isr_gpio1() {
  if ( isr_gpiox(0) )
    portYIELD_FROM_ISR();
}

// ISR specific to Button 2

static void IRAM_ATTR isr_gpio2() {
  if ( isr_gpiox(1) )
    portYIELD_FROM_ISR();
}

// ISR specific to Button 3

static void IRAM_ATTR isr_gpio3() {
  if ( isr_gpiox(2) )
    portYIELD_FROM_ISR();
}

// Program Initialization

void setup() {
  int app_cpu = xPortGetCoreID();
  QueueSetHandle_t hqset;
  BaseType_t rc;

  hqset = xQueueCreateSet(Q_DEPTH*N_BUTTONS);
  assert(hqset);

  // For each button + LED pair:
  for ( unsigned ux=0; ux<N_BUTTONS; ++ux ) {
    s_button& button = buttons[ux];

    button.qh = xQueueCreate(Q_DEPTH,sizeof(bool));
    assert(button.qh);
    rc = xQueueAddToSet(button.qh,hqset);
    assert(rc == pdPASS);
    pinMode(button.led_gpio,OUTPUT);
    digitalWrite(button.led_gpio,1);
    pinMode(button.butn_gpio,INPUT_PULLUP);
    attachInterrupt(button.butn_gpio,button.isr,CHANGE);
  }

  // Start the event task
  rc = xTaskCreatePinnedToCore(
    evtask,     // Function
    "evtask",   // Name
    4096,       // Stack size
    (void*)hqset, // Argument
    1,          // Priority
    nullptr,    // Handle ptr
    app_cpu     // CPU
  );
  assert(rc == pdPASS);
}

// Not used

void loop() {
  vTaskDelete(nullptr);
}
