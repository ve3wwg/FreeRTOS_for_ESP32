// worms1.ino
// MIT License (see file LICENSE)

// Worm task priorities
#define WORM1_TASK_PRIORITY 9
#define WORM2_TASK_PRIORITY 8
#define WORM3_TASK_PRIORITY 7

// loop() must have highest priority
#define MAIN_TASK_PRIORITY  10

#include "SSD1306.h"

class Display : public SSD1306 {
  int w, h; // Width, height
public:	
  Display(
    int width=128,
    int height=64,
    int addr=0x3C,
    int sda=5,
    int scl=4);
  int width() { return w; }
  int height() { return h; }
  void lock();
  void unlock();
  void clear();
  void init();
};

class InchWorm {
  static const int segw = 9, segsw = 4, segh = 3;
  Display& disp;
  int worm;
  int x, y; // Coordinates of worm (left)
  int wormw=30;
  int wormh=10;
  int dir=1;  // Direction
  int state=0;
public: 
  InchWorm(Display& disp,int worm);
  void draw();
};

InchWorm::InchWorm(Display& disp,int worm)
: disp(disp), worm(worm) {
}

void
InchWorm::draw() {
  int py = 7 + (worm-1) * 20;
  int px = 2 + x;
 
  py += wormh - 3;

  disp.setColor(WHITE);
  disp.fillRect(px,py-2*segh,3*segw,3*segh);
  disp.setColor(BLACK);
    
  switch ( state ) {
    case 0: // _-_
      disp.fillRect(px,py,segw,segh);
      disp.fillRect(px+segw,py-segh,segsw,segh);
      disp.fillRect(px+segw+segsw,py,segw,segh);
      break;
    case 1: // _^_ (high hump)
      disp.fillRect(px,py,segw,segh);
      disp.fillRect(px+segw,py-2*segh,segsw,segh);
      disp.fillRect(px+segw+segsw,py,segw,segh);
      disp.drawLine(px+segw,py,px+segw,py-2*segh);
      disp.drawLine(px+segw+segsw,py,px+segw+segsw,py-2*segh);
      break;
    case 2: // _^^_ (high hump, stretched)
      if ( dir < 0 )
        px -= segsw;
      disp.fillRect(px,py,segw,segh);
      disp.fillRect(px+segw,py-2*segh,segw,segh);
      disp.fillRect(px+2*segw,py,segw,segh);
      disp.drawLine(px+segw,py,px+segw,py-2*segh);
      disp.drawLine(px+2*segw,py,px+2*segw,py-2*segh);
      break;
    case 3: // _-_ (moved)
      if ( dir < 0 )
        px -= segsw;
      else
        px += segsw;
      disp.fillRect(px,py,segw,segh);
      disp.fillRect(px+segw,py-segh,segsw,segh);
      disp.fillRect(px+segw+segsw,py,segw,segh);
      break;
    default:
      ;
  }
  state = (state+1) % 4;
  if ( !state ) {
    x += dir*segsw;
    if ( dir > 0 ) {
      if ( x + 3*segw+segsw >= disp.width() )
        dir = -1;
    } else if ( x <= 2 )
      dir = +1;
  }
  disp.display();
}

Display::Display(
  int width,
  int height,
  int addr,
  int sda,
  int scl)
: w(width), h(height),
  SSD1306(addr,sda,scl) {
}

void
Display::init() {
  SSD1306::init();
  clear();
  flipScreenVertically();
  display();
}

void
Display::clear() {
  SSD1306::clear();
  setColor(WHITE);
  fillRect(0,0,w,h);
  setColor(BLACK);
}

static Display oled;
static InchWorm worm1(oled,1);
static InchWorm worm2(oled,2);
static InchWorm worm3(oled,3);
static QueueHandle_t qh = 0;
static int app_cpu = 0; // Updated by setup()

void worm_task(void *arg) {
  InchWorm *worm = (InchWorm*)arg;
  
  for (;;) {
    for ( int x=0; x<800000; ++x )
      __asm__ __volatile__("nop");
    xQueueSendToBack(qh,&worm,0);
    // vTaskDelay(10);
  }
}

void setup() {
  TaskHandle_t h = xTaskGetCurrentTaskHandle();

  app_cpu = xPortGetCoreID(); // Which CPU?
  oled.init();
  vTaskPrioritySet(h,MAIN_TASK_PRIORITY); 
  qh = xQueueCreate(4,sizeof(InchWorm*));

  // Draw at least one worm each:
  worm1.draw();
  worm2.draw();
  worm3.draw();

  xTaskCreatePinnedToCore(
    worm_task,  // Function
    "worm1",    // Task name
    3000,       // Stack size
    &worm1,     // Argument
    WORM1_TASK_PRIORITY,
    nullptr,    // No handle returned
    app_cpu);
    
  xTaskCreatePinnedToCore(
    worm_task,  // Function
    "worm2",    // Task name
    3000,       // Stack size
    &worm2,     // Argument
    WORM2_TASK_PRIORITY,
    nullptr,    // No handle returned
    app_cpu);

  xTaskCreatePinnedToCore(
    worm_task,  // Function
    "worm3",    // Task name
    3000,       // Stack size
    &worm3,     // Argument
    WORM3_TASK_PRIORITY,
    nullptr,    // No handle returned
    app_cpu);

  delay(1000);  // Allow USB to connect
  printf("worms1.ino: CPU %d\n",app_cpu);
}

void loop() {
  InchWorm *worm = nullptr;

  if ( xQueueReceive(qh,&worm,1) == pdPASS )
    worm->draw();
  else
    delay(1);
}
