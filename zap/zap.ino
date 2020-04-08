// zap.ino 
// MIT License (see file LICENSE)

// LEDs are active low:
// Player Red's LED and button
#define GPIO_RED    12 
#define BUTTON_RED  25

// Player Blue's LED and button
#define GPIO_BLUE   13
#define BUTTON_BLUE 26

// Button Event
struct s_bevent {
  uint8_t   playerx;  // Player index
  bool      press;    // True when press
};

// Player Config
static struct s_player {
  uint8_t   playerx;  // Player index
  uint8_t   led;      // GPIO of LED
  uint8_t   button;   // GPIO of button
  bool      ready;    // Player ready
} players[2] = {
  { 0u, GPIO_RED, BUTTON_RED, false },
  { 1u, GPIO_BLUE, BUTTON_BLUE, false },
};

static unsigned seedv = 0u; // For rand_r()
static const int button_timeout = -1;

// Button Event Queue
static QueueHandle_t bqueue;

//
// Return a queued button event:
//  true  Button press event
//  false Timed out        
//
static int rdbutton(s_bevent *pevent) {

  switch ( xQueueReceive(bqueue,pevent,0) ) {
  case pdPASS:
    if ( !pevent->press )
      taskYIELD();
    return 1;
  case errQUEUE_EMPTY:
    taskYIELD();
    return false;
  }
}

//
// Button Debouncing task:
//
static void debounce(void *argp) {
  s_player *pplayer = (s_player*)argp;
  uint8_t level, state = 0;
  uint8_t mask = 0b00001111;
  s_bevent event;
  
  assert(bqueue);

  for (;;) {
    level = !!digitalRead(pplayer->button);
    state = (state << 1) | level;
    if ( (state & mask) == mask
      || (state & mask) == 0 ) {
      // Queue up button press/release
      event.playerx = pplayer->playerx;
      event.press = ((state & mask) == mask) ^ 1;
      xQueueSendToBack(bqueue,&event,0);
    }
    taskYIELD();
  }
}

//
// Indicate with LED the player that got
// Zapped!
//
static void zap(uint8_t playerx) {

  // All LEDs off:
  for ( auto& player : players )
    digitalWrite(player.led,HIGH);
  // One LED on:
  digitalWrite(players[playerx].led,LOW);
printf("Zapped: playerx=%u\n",playerx);
}

//
// Zap all players, except the winner:
//
static void zapo(uint8_t playerx) {

  // All but playerx have LED on (zapped)
  for ( auto& player : players ) {
    digitalWrite(player.led,
      player.playerx == playerx
      ? HIGH
      : LOW
    );
  }
printf("All but playerx=%u zapped!\n",playerx);
}

//
// Randomize the player LEDs until
// zap time
//
static void pending() {
  unsigned count = 0;
  int r;
  
  for ( auto& player : players ) {
    r = rand_r(&seedv) & 1;
    digitalWrite(player.led,r ? HIGH : LOW);
    if ( r )
      ++count;
  }

  if ( !count ) {
    // At least one LED must be lit
    r = rand_r(&seedv) % (sizeof players/sizeof players[0]);
    digitalWrite(players[r].led,LOW);
  }
}

static void reset_game() {
  s_bevent event;
  unsigned count, readyc;
  bool readyf;

  delay(2000);
  printf("Game Reset:\n");

  // Wait for buttons to idle
  do  {
    count = 0;

    for ( auto& player : players ) {
      player.ready = false;
      if ( !rdbutton(&event) )
        continue;
      digitalWrite(player.led,!event.press);
      if ( !event.press )
        ++count;  // Released button
    }
  } while ( count > 0 );

  xQueueReset(bqueue);

  // Wait for check in
  printf("Waiting for player checkin..\n");

  do {
    for ( auto& player : players ) {
      if ( !rdbutton(&event) || !event.press )
        continue;
      player.ready = true;
    }
    readyf = true;
    for ( auto& player : players ) {
      if ( !player.ready )
        readyf = false;
    }
  } while ( !readyf );

  delay(3000);
  xQueueReset(bqueue);
}

//
// Game controller:
//  1. Randomly set zap time (to a
//     minimum of 2 seconds from now)
//  2. Wait until random time has 
//     elapsed, but if anyone presses
//     a button at this time, they are
//     zapped.
//  3. Time expires: LEDs all dark!
//  4. First player to button press
//     is spared (LEDs of losers are
//     lit, implying a ZAP!)
//  5. If no one responds within 3
//     seconds, then all players get
//     zapped.
//
static void controller(void *argp) {
  BaseType_t button, playerx;
  unsigned long time1, time2;
  s_bevent event;

  assert(bqueue);

  for (;;) {
    reset_game();
    printf("Let the games begin!\n\n");

    time1 = millis()        // Zap time
      + 2000ul
      + rand_r(&seedv) % 10000ul;
    time2 = time1 + 5000ul; // Timeout

    // Loop until zap time..
printf("Zap is coming...\n");
    while ( millis() < time1 ) {
      pending();            // Randomize LEDs

      // Test for button press
      if ( !rdbutton(&event) || !event.press )
        continue;

printf("Button for playerx = %d was pressed!\n",playerx);
      zap(event.playerx); // Player gets zapped
      break;
    }

    if ( event.press )
      continue;

    // Darken all LEDs to announce Zap!
    for ( auto& player : players )
      digitalWrite(player.led,HIGH);

printf("Zap!! Waiting for a button press...\n");

    // Until a button press
    while ( millis() < time2 ) {
      if ( !rdbutton(&event) || !event.press )
        continue;

printf("Finally button for playerx = %d was pressed!\n",event.playerx);
      zapo(event.playerx);
      break;
    }

    if ( !event.press ) {
printf("Zap ALL!!\n");
      zapo(255); // Zap all!
    }
  }
}

// Program initialization:
void setup() {
  int app_cpu = xPortGetCoreID();
  TaskHandle_t h;

  delay(2000);          // Allow USB connect
  seedv = hallRead();   // Random seed value
  bqueue = xQueueCreate(4,sizeof(s_bevent));
  assert(bqueue != nullptr);

  for ( auto& player : players ) {
    pinMode(player.led,OUTPUT);
    digitalWrite(player.led,HIGH);
    pinMode(player.button,INPUT_PULLUP);
    xTaskCreatePinnedToCore(
      debounce,
      "debounce",
      2048,     // Stack size
      &player,  // &players[x]
      1,        // Priority
      &h,       // Task handle
      app_cpu   // CPU
    );
    assert(h != nullptr);
  }

  xTaskCreatePinnedToCore(
    controller,
    "controller",
    2048,     // Stack size
    nullptr,  // Not used
    1,        // Priority
    &h,       // Task handle
    app_cpu   // CPU
  );
  assert(h);

  printf("Let the games begin!\n");
}

// Not used:
void loop() {
  vTaskDelete(nullptr);
}
