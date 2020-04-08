// pcf8563.ino 

// Set to zero if using PWM
// instead of crystal. Wire
// GPIO PWM_GPIO to CLKIN
// when not using crystal.

#define WITH_XTAL     1

// GPIOs used for I2C

#define I2C_SDA       25
#define I2C_SCL       26

// GPIOs used for input interrupts

#define GPIO_PULSEIN  14
#define GPIO_INT      12

#if !WITH_XTAL
// GPIO for PWM output
#define PWM_GPIO      16
#define PWM_CH        0
#define PWM_FREQ      32768
#define PWM_RES       8
#endif

#include <stdarg.h>
#include <Wire.h>

// PCF8563 Register Definitions

#define I2C_DEV       0x51
#define REG_CTL1      0x00
#define REG_CTL2      0x01
#define REG_VLSEC     0x02
#define REG_MALRM     0X09
#define REG_CLKOUT    0x0D

static int app_cpu = 0;

static SemaphoreHandle_t hpulse;
static SemaphoreHandle_t hint;
static SemaphoreHandle_t hserial;
static SemaphoreHandle_t hi2c;
static QueueSetHandle_t hqset;

////////////////////////////////////////
// Interrupt service for /INT
////////////////////////////////////////

void IRAM_ATTR isr_int() {
  BaseType_t woken = pdFALSE;

  xSemaphoreGiveFromISR(hint,&woken);
  // ignore return value
  if ( woken )
    portYIELD_FROM_ISR();
}

////////////////////////////////////////
// Interrupt service for CLKOUT
////////////////////////////////////////

void IRAM_ATTR isr_pulse() {
  BaseType_t woken = pdFALSE;

  xSemaphoreGiveFromISR(hpulse,&woken);
  // ignore return value
  if ( woken )
    portYIELD_FROM_ISR();
}

////////////////////////////////////////
// Locking for I2C Bus
////////////////////////////////////////

static void lock_i2c() {
  BaseType_t rc
    = xSemaphoreTakeRecursive(hi2c,portMAX_DELAY);
  assert(rc == pdPASS);
}

static void unlock_i2c() {
  BaseType_t rc
    = xSemaphoreGiveRecursive(hi2c);
  assert(rc == pdPASS);
}

////////////////////////////////////////
// Locking for Serial out
////////////////////////////////////////

static void lock_serial() {
  BaseType_t rc
    = xSemaphoreTakeRecursive(hserial,portMAX_DELAY);
  assert(rc == pdPASS);
}

static void unlock_serial() {
  BaseType_t rc
    = xSemaphoreGiveRecursive(hserial);
  assert(rc == pdPASS);
}

////////////////////////////////////////
// Self-locking printf()
////////////////////////////////////////

void logf(const char *format,...) {
  va_list ap;

  lock_serial();
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
  unlock_serial();
}

////////////////////////////////////////
// Read 1 byte from I2C Register
////////////////////////////////////////

int read_i2c(uint8_t reg) {
  int rc, v;

  lock_i2c();
  Wire.beginTransmission(I2C_DEV);
  Wire.write(reg);
  Wire.endTransmission(false);
  rc = Wire.requestFrom(uint16_t(I2C_DEV),1u,true);
  if ( rc == 1 )
    v = Wire.read();
  else
    v = -1;
  Wire.endTransmission();
  unlock_i2c();
  return v;
}

////////////////////////////////////////
// Read n bytes starting with I2C register
////////////////////////////////////////

int read_i2c(uint8_t reg,uint8_t *buf,unsigned n) {
  int rc;

  for ( int x=0; x<n; ++x )
    buf[x] = 0;
  lock_i2c();
  Wire.beginTransmission(I2C_DEV);
  Wire.write(reg);
  Wire.endTransmission(false);
  rc = Wire.requestFrom(uint16_t(I2C_DEV),n,true);
  for ( int x=0; x<rc; ++x )
    buf[x] = Wire.read();
  unlock_i2c();
  return rc;
}

////////////////////////////////////////
// Write one byte to I2C register
////////////////////////////////////////

void write_i2c(uint8_t reg,uint8_t v) {
  lock_i2c();
  Wire.beginTransmission(I2C_DEV);
  Wire.write(reg);
  Wire.write(v);
  Wire.endTransmission();
  unlock_i2c();
}

////////////////////////////////////////
// Writing n zero bytes starting at register
////////////////////////////////////////

void zero_i2c(uint8_t reg,unsigned n) {
  lock_i2c();
  Wire.beginTransmission(I2C_DEV);
  Wire.write(reg);
  for ( unsigned ux=0; ux<n; ++ux )
    Wire.write(0x00);
  Wire.endTransmission();
  unlock_i2c();
}

////////////////////////////////////////
// Monitor Task using Queue Set
////////////////////////////////////////

void monitor(void *arg) {
  static uint8_t mask[16] = {
    0xAF, 0x1F,
    0x7F, 0x7F, 0x3F, 0x3F,
    0x07, 0x1F, 0xFF, 0xFF, 
    0xBF, 0xBF, 0xB7, 0x83,
    0x83, 0xFF
  };
  uint8_t buf[16];
  QueueSetMemberHandle_t h;
  int b, n;
  BaseType_t rc;

  for (;;) {
    // Block until hpulse or hint
    h = xQueueSelectFromSet(hqset,portMAX_DELAY);

    if ( h == hpulse ) {
      // Pulse from PCF8563 CLKOUT
      rc = xSemaphoreTake(h,portMAX_DELAY);
      assert(rc == pdPASS);
      n = read_i2c(REG_CTL1,buf,sizeof buf);
      lock_serial();
      logf("buf[%d]: ",n);
      for ( unsigned ux=0; ux<sizeof buf; ++ux ) {
        buf[ux] &= mask[ux];
        logf("%02X ",buf[ux]);
      }
      logf("\n");
      unlock_serial();
    } else if ( h == hint ) {
      // Interrupt pin /INT was active:
      rc = xSemaphoreTake(h,portMAX_DELAY);
      assert(rc == pdPASS);
      b = read_i2c(REG_CTL2);
      if ( b & 0b00001000 ) {
        // Clear alarm flag
        logf(" (ALARM)\n");
        write_i2c(REG_CTL1,0x00010110);
      } else  {
        // This happens at startup
        logf(" (false alarm)\n");
      }
    }
  }
}

////////////////////////////////////////
// Initialization
////////////////////////////////////////

void setup() {
  unsigned ms;
  BaseType_t rc;  // Return code
  uint8_t b;

  app_cpu = xPortGetCoreID();

  // Create semaphores and mutexes
  hpulse = xSemaphoreCreateBinary();
  assert(hpulse);
  hint = xSemaphoreCreateBinary();
  assert(hint);
  hserial = xSemaphoreCreateMutex();
  assert(hserial);
  hi2c = xSemaphoreCreateMutex();
  assert(hi2c);

  // Create queue set
  hqset = xQueueCreateSet(16);
  assert(hqset);
  rc = xQueueAddToSet(hpulse,hqset);
  assert(rc == pdPASS);
  rc = xQueueAddToSet(hint,hqset);
  assert(rc == pdPASS);

  // Start I2C Bus Support:
  Wire.begin(I2C_SDA,I2C_SCL);

  // Interrupt on PULSEIN for CLKOUT
  pinMode(GPIO_PULSEIN,INPUT_PULLUP);
  attachInterrupt(GPIO_PULSEIN,isr_pulse,RISING);

  // Interrupt on INT for /INT
  pinMode(GPIO_INT,INPUT_PULLUP);
  attachInterrupt(GPIO_INT,isr_int,FALLING);

  // Allow USB Serial time to start
  delay(2000);
  logf("\npcf8563.ino:\n");

#if !WITH_XTAL
  // Use PWM to drive CLKIN
  logf("without xtal..\n");
  ledcSetup(PWM_CH,PWM_FREQ,PWM_RES);
  ledcAttachPin(PWM_GPIO,PWM_CH);
  ledcWrite(PWM_CH,127); // 50%
#else
  // Using a 32kHz crystal
  logf("with xtal..\n");
#endif

  // Must allow PCF8563 some time
  // to startup its clock circuits
  delay(1000);

  // Can we reach the PCF8563?
  if ( read_i2c(REG_CTL1) == -1 ) {
    logf("PCF8563 chip did not respond.\n");
    vTaskDelete(nullptr);
  }

  // Start clock in normal mode
  write_i2c(REG_CTL1,0x00);

  // Clear alarm flags
  write_i2c(REG_CTL2,0b00010010);

  // Start PCF8563 CLKOUT pin
  write_i2c(REG_CLKOUT,0b10000011);

  // Clear time and date
  zero_i2c(REG_VLSEC,7);

  // Set 1 minute alarm
  write_i2c(REG_MALRM,0x01);
  write_i2c(REG_CTL2,0b00010010);

  // Start the monitor task
  rc = xTaskCreatePinnedToCore(
    monitor,    // Function
    "monitor",  // Name
    4096,       // Stack size
    nullptr,    // Argument
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
