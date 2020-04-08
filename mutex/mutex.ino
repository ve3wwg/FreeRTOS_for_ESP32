// mutex.ino 

// GPIOs used for I2C
#define I2C_SDA       25
#define I2C_SCL       26

// Set to 1 for PCF8574A
#define PCF8574A      0

#include <Wire.h>

#if PCF8574A
// Newer PCF8574A addresses
#define DEV0          0x38
#define DEV1          0x39
#else
// Original PCF8574 addresses
#define DEV0          0x20
#define DEV1          0x21
#endif

static int app_cpu = 0;
static SemaphoreHandle_t mutex;
static int pcf8574_1 = DEV0;
static int pcf8574_2 = DEV1;

//
// Lock I2C Bus with mutex
//
static void lock_i2c() {
  BaseType_t rc;

  rc = xSemaphoreTake(mutex,portMAX_DELAY);
  assert(rc == pdPASS);
}

//
// Unlock I2C Bus with mutex
//
static void unlock_i2c() {
  BaseType_t rc;

  rc = xSemaphoreGive(mutex);
  assert(rc == pdPASS);
}

//
// I2C extender blink task:
//
static void led_task(void *argp) {
  int i2c_addr = *(unsigned*)argp;
  bool led_status = false;
  int rc;

  lock_i2c();
  printf("Testing I2C address 0x%02X\n",
    i2c_addr);
  Wire.begin();
  Wire.requestFrom(i2c_addr,1);
  rc = Wire.available();
  if ( rc > 0 ) {
    Wire.read();
    Wire.beginTransmission(i2c_addr);
    Wire.write(0xFF); // All GPIOs high
    Wire.endTransmission();
    printf("I2C address 0x%02X present.\n",
      i2c_addr);
  } else  {
    printf("I2C address 0x%02X not responding.\n",
      i2c_addr);
  }
  unlock_i2c();  

  if ( rc <= 0 ) {
    // Cancel task if I2C fail
    vTaskDelete(nullptr);
  }

  //
  // Blink loop
  //
  for (;;) {
    lock_i2c();
    led_status ^= true;
    printf("LED 0x%02X %s\n",
      i2c_addr,
      led_status ? "on" : "off");
    Wire.beginTransmission(i2c_addr);
    Wire.write(led_status ? 0b11110111 : 0b11111111);
    Wire.endTransmission();
    unlock_i2c();

    // Use different delays per task
    delay(i2c_addr & 1 ? 500 : 600);
  }
}

//
// Initialize
//
void setup() {
  BaseType_t rc;  // Return code

  app_cpu = xPortGetCoreID();

  // Create mutex
  mutex = xSemaphoreCreateMutex();
  assert(mutex);

  // Start I2C Bus Support:
  Wire.begin(I2C_SDA,I2C_SCL);

  delay(2000);  // Allow USB to connect
  printf("\nmutex.ino:\n");

  rc = xTaskCreatePinnedToCore(
    led_task,   // Function
    "led_task1",// Name
    2000,       // Stack size
    &pcf8574_1, // Argument
    1,          // Priority
    nullptr,    // Handle ptr
    app_cpu     // CPU
  );
  assert(rc == pdPASS);

  rc = xTaskCreatePinnedToCore(
    led_task,   // Function
    "led_task2",// Name
    2000,       // Stack size
    &pcf8574_2, // Argument
    1,          // Priority
    nullptr,    // Handle ptr
    app_cpu     // CPU
  );
  assert(rc == pdPASS);
}

// Not used:
void loop() {
  vTaskDelete(nullptr);
}
