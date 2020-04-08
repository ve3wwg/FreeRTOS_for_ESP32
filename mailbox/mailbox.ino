// mailbox.ino 
// MIT License (see file LICENSE)

// Configuration
#define I2C_SDA       25
#define I2C_SCL       26
#define DEV_Si7021    (uint8_t(0x1E))
#define DEV_HMC5883L  (uint8_t(0x40))

#include <Wire.h>
#include <Adafruit_Si7021.h>

static Adafruit_Si7021 si7021;

static int app_cpu = 0;
static QueueHandle_t comph = nullptr;
static QueueHandle_t temph = nullptr;
static SemaphoreHandle_t chsem = nullptr;
static SemaphoreHandle_t i2sem = nullptr;

struct s_compass {
  uint16_t  x;
  uint16_t  y;
  uint16_t  z;
};

struct s_temp {
  float     temp;
  float     humidity;
};

//
// Lock I2C Bus
//
static inline void i2c_lock() {
  BaseType_t rc;

  rc =  xSemaphoreTake(i2sem,portMAX_DELAY);
  assert(rc == pdPASS);
}

//
// Unlock I2C Bus
//
static inline void i2c_unlock() {
  BaseType_t rc;

  rc = xSemaphoreGive(i2sem);
  assert(rc == pdPASS);
}

//
// Temperature and Humidity Task
//
static void temp_task(void *argp) {
  s_temp reading;
  uint8_t er;
  BaseType_t rc;

  i2c_lock();

  if ( !si7021.begin() ) {
    i2c_unlock();
    vTaskDelete(nullptr);
  }

  si7021.reset();
  i2c_unlock();

  reading.temp = 0.0;
  reading.humidity = 0.0;

  for (;;) {
    i2c_lock();
    reading.temp = si7021.readTemperature();
    reading.humidity = si7021.readHumidity();
    i2c_unlock();

    rc = xQueueOverwrite(temph,&reading);
    assert(rc == pdPASS);

    // Notify disp_task:
    xSemaphoreGive(chsem);

    delay(500);
  }
}

//
// Read MSB + LSB for compass reading
//
static inline int16_t read_i16() {
  uint8_t ub1, ub2;

  ub1 = Wire.read();
  ub2 = Wire.read();
  return int16_t((ub1 << 8)|ub2);
}

//
// Compass reading task:
//
static void comp_task(void *argp) {
  s_compass reading;
  BaseType_t rc;
  int16_t i16;
  uint8_t s;
  bool status;

  for (;;) {
    status = false;

    i2c_lock();
    Wire.beginTransmission(DEV_HMC5883L);
    Wire.write(9);  // Status register
    rc = Wire.requestFrom(DEV_HMC5883L,uint8_t(1));
    if ( rc != 1 ) {
      i2c_unlock();
      printf("I2C Fail for HMC5883L\n");
      sleep(1000);
      continue;
    }

    s = Wire.read();  // Status
    i2c_unlock();

    if ( !(s & 0x01) )
      continue;

    // Device is ready for reading
    i2c_lock();
    Wire.beginTransmission(DEV_HMC5883L);
    Wire.write(3);
    rc = Wire.requestFrom(DEV_HMC5883L,uint8_t(6));
    if ( rc == 6 ) {
      reading.x = read_i16();
      reading.z = read_i16();
      reading.y = read_i16();
      status = true;
    }
    i2c_unlock();

    if ( status ) {
      rc = xQueueOverwrite(comph,&reading);
      assert(rc == pdPASS);

      // Notify disp_task:
      xSemaphoreGive(chsem);
    }
    delay(500);
  }
}

//
// Display task (Serial Monitor)
//
static void disp_task(void *argp) {
  s_temp temp_reading;
  s_compass comp_reading;
  BaseType_t rc;

  for (;;) {
    // Wait for change notification:
    rc = xSemaphoreTake(chsem,portMAX_DELAY);
    assert(rc == pdPASS);
    
    // Grab temperature, if any:
    rc = xQueuePeek(temph,&temp_reading,0);
    if ( rc == pdPASS ) {
      printf("Temperature:      %.2fC, RH %.2f %%\n",
        temp_reading.temp,
        temp_reading.humidity);
    } else {
      printf("Temperature & RH not available.\n");
    }

    // Grab compass readings, if any:
    rc = xQueuePeek(comph,&comp_reading,0);
    if ( rc == pdPASS ) {
      printf("Compass readings: %d, %d, %d\n",
        comp_reading.x,
        comp_reading.y,
        comp_reading.z);
    } else {
      printf("Compass reading not available.\n");
    }
  }
}

//
// Program Initialization
//
void setup() {
  BaseType_t rc;

  app_cpu = xPortGetCoreID();

  // Change notification:
  chsem = xSemaphoreCreateBinary();
  assert(chsem);

  // I2C locking semaphore:
  i2sem = xSemaphoreCreateBinary();
  assert(i2sem);
  rc = xSemaphoreGive(i2sem);
  assert(rc == pdPASS);

  // Compass Mailbox:
  comph = xQueueCreate(1,sizeof(s_compass));
  assert(comph);

  // Temperature and RH Mailbox:
  temph = xQueueCreate(1,sizeof(s_temp));
  assert(temph);

  // Start I2C Bus Support:
  Wire.begin(I2C_SDA,I2C_SCL);

  // Allow USB to Serial to start:
  delay(2000);
  printf("\nmailbox.ino:\n");

  // Temperature Reading Task:
  rc = xTaskCreatePinnedToCore(
    temp_task,
    "temptsk",
    2400,     // Stack size
    nullptr,
    1,        // Priority
    nullptr,  // Task handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);

  // Compass Reading Task:
  rc = xTaskCreatePinnedToCore(
    comp_task,
    "comptsk",
    2400,     // Stack size
    nullptr,
    1,        // Priority
    nullptr,  // Task handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);

  // Display task:
  rc = xTaskCreatePinnedToCore(
    disp_task,
    "disptsk",
    4000,     // Stack size
    nullptr,  
    1,        // Priority
    nullptr,  // Task handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);
}

// Not used:
void loop() {
  vTaskDelete(nullptr);
}
