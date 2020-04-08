// i2cscan.ino 
// MIT License (see file LICENSE)

#define I2C_SDA       25
#define I2C_SCL       26

#include <Wire.h>

void setup() {
  BaseType_t rc;

  Wire.begin(I2C_SDA,I2C_SCL);

  delay(2000); // Allow USB to connect
  printf("\ni2cscan.ino:\n");
}

void loop() {
  uint8_t error;
  int count = 0;

  printf("\nScanning I2C bus SDA=%u, SCL=%u...\n",
    I2C_SDA,I2C_SCL);

  for ( uint8_t addr=1; addr < 127; ++addr ) {
    Wire.beginTransmission(addr);
    error = Wire.endTransmission();
    if ( !error ) {
      printf("  device found at 0x%02X\n",addr);
      ++count;
    } else if ( error == 4 ) {
      printf("  Error at 0x%02X\n",addr);
    }    
  }

  if ( !count )
    printf("No I2C devices found\n");
  else
    printf("%d devices found.\n",count);
  delay(5000);          
}
