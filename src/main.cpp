#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include "pico/time.h"
#include "i2c_flexigpio.h"

// RPI Pico

//--------------------------------------------------------------------+
// setup() & loop()
//--------------------------------------------------------------------+
void setup() {
  // stdio_init_all();

  // // Wait for USB to connect (important in VS Code builds)
  // while (!stdio_usb_connected()) {
  //     sleep_ms(100);
  // }

  // printf("FlexiGPIO I2C Expander 1.1\n");

  init_i2c_responder();
}

void loop() {
  i2c_task();
}

int main() {
  setup();
  while (true) {
    loop();
  }
  return 0;
}
