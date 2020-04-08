// countsem.ino 
// The Dining Philosophers Problem
// MIT License (see file LICENSE)

#define PREVENT_DEADLOCK 1
#define N                4
#define N_EATERS         (N-1)

static QueueHandle_t     msgq;
static SemaphoreHandle_t csem;
static int app_cpu = 0;

enum State {
  Thinking=0,
  Hungry,
  Eating
};

static const char *state_name[] = {
  "Thinking",
  "Hungry",
  "Eating"
};

struct s_philosopher {
  TaskHandle_t  task;
  unsigned      num;
  State         state;
  unsigned      seed;
};

struct s_message {
  unsigned      num;
  State         state;
};

static s_philosopher philosophers[N];
static SemaphoreHandle_t forks[N];
static volatile unsigned logno = 0;

//
// Send the P. state by queue
//
static void send_state(s_philosopher *philo) {
  s_message msg;
  BaseType_t rc;

  msg.num = philo->num;
  msg.state = philo->state;
  rc = xQueueSendToBack(msgq,&msg,portMAX_DELAY);
}

//
// The Philosopher task
//
static void philo_task(void *arg) {
  s_philosopher *philo = (s_philosopher*)arg;
  SemaphoreHandle_t fork1=0, fork2=0;
  BaseType_t rc;

  delay(rand_r(&philo->seed)%5+1);

  for (;;) {
    philo->state = Thinking;
    send_state(philo);
    delay(rand_r(&philo->seed)%5+1);

    philo->state = Hungry;
    send_state(philo);
    delay(rand_r(&philo->seed)%5+1);

#if PREVENT_DEADLOCK
    rc = xSemaphoreTake(csem,portMAX_DELAY);
    assert(rc == pdPASS);
#endif

    // Pick up forks:
    fork1 = forks[philo->num];
    fork2 = forks[(philo->num+1) % N];
    rc = xSemaphoreTake(fork1,portMAX_DELAY);
    assert(rc == pdPASS);
    delay(rand_r(&philo->seed)%5+1);
    rc = xSemaphoreTake(fork2,portMAX_DELAY);
    assert(rc == pdPASS);

    philo->state = Eating;
    send_state(philo);
    delay(rand_r(&philo->seed)%5+1);

    // Put down forks:
    rc = xSemaphoreGive(fork1);
    assert(rc == pdPASS);
    delay(1);
    rc = xSemaphoreGive(fork2);
    assert(rc == pdPASS);

#if PREVENT_DEADLOCK
    rc = xSemaphoreGive(csem);
    assert(rc == pdPASS);
#endif
  }
}

//
// Program Initialization
//
void setup() {
  BaseType_t rc;

  app_cpu = xPortGetCoreID();
  msgq = xQueueCreate(30,sizeof(s_message));
  assert(msgq);

  for ( unsigned x=0; x<N; ++x ) {
    forks[x] = xSemaphoreCreateBinary();
    assert(forks[x]);
    rc = xSemaphoreGive(forks[x]);
    assert(rc == pdPASS);
    assert(forks[x]);
  }

  delay(2000); // Allow USB to connect
  printf("\nThe Dining Philosopher's Problem:\n");
  printf("There are %u Philosophers.\n",N);

#if PREVENT_DEADLOCK
  csem = xSemaphoreCreateCounting(
    N_EATERS,
    N_EATERS
  );
  assert(csem);
  printf("With deadlock prevention.\n");
#else
  csem = nullptr;
  printf("Without deadlock prevention.\n");
#endif

  // Initialize for tasks:
  for ( unsigned x=0; x<N; ++x ) {
    philosophers[x].num = x;
    philosophers[x].state = Thinking;
    // philosophers[x].seed = hallRead();
    philosophers[x].seed = 7369+x;
  }

  // Create philosopher tasks:
  for ( unsigned x=0; x<N; ++x ) {
    rc = xTaskCreatePinnedToCore(
      philo_task,
      "philotsk",
      5000,             // Stack size
      &philosophers[x], // Parameters
      1,                // Priority
      &philosophers[x].task, // handle
      app_cpu           // CPU
    );
    assert(rc == pdPASS);
    assert(philosophers[x].task);
  }
}

//
// Report philosopher states:
//
void loop() {
  s_message msg;

  while ( xQueueReceive(msgq,&msg,1) == pdPASS ) {
    printf("%05u: Philosopher %u is %s\n",
      ++logno,
      msg.num,
      state_name[msg.state]);
  }
  delay(1);
}
