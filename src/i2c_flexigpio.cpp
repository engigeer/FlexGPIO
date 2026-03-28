
#include "i2c_flexigpio.h"

/**
 * Example program for basic use of pico as an I2C peripheral (previously known as I2C slave)
 * 
 * This example allows the pico to act as a 256byte RAM
 * 
 * Author: Graham Smith (graham@smithg.co.uk)
 */


// Usage:
//
// When writing data to the pico the first data byte updates the current address to be used when writing or reading from the RAM
// Subsequent data bytes contain data that is written to the ram at the current address and following locations (current address auto increments)
//
// When reading data from the pico the first data byte returned will be from the ram storage located at current address
// Subsequent bytes will be returned from the following ram locations (again current address auto increments)
//
// N.B. if the current address reaches 255, it will autoincrement to 0 after next read / write

// define I2C addresses to be used for this peripheral
static const uint I2C_SLAVE_ADDRESS = 0x48;
static const uint I2C_BAUDRATE = 1000000; // 1000 kHz

// GPIO pins to use for I2C SLAVE
static const uint I2C_SLAVE_SDA_PIN = 0;
static const uint I2C_SLAVE_SCL_PIN = 1;

//flash defines
#define FLASH_TARGET_OFFSET (256 * 1024)

const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);
status_context_t input_context, output_context;
input_packet_t &inputpacket = *reinterpret_cast<input_packet_t*>(&input_context.mem[0]);
output_packet_t &outputpacket = *reinterpret_cast<output_packet_t*>(&output_context.mem[0]);

uint8_t mem_address = 0;
uint8_t mem_address_written = 0;

//char *ram_ptr = (char*) &context.mem[0];
int character_sent;
int command_error = 0;

const uint32_t input_pin_mask = 
    (1u << TOOL_PIN  )|
    (1u << PROBE_PIN )|
    (1u << ALARMX_PIN)|
    (1u << ALARMY_PIN)| 
    (1u << ALARMZ_PIN)| 
    (1u << ALARMA_PIN)| 
    (1u << ALARMB_PIN)| 
    (1u << ALARMC_PIN);

const uint32_t comm_pin_mask = 
    (1u << MCU_PRB_PIN)|
    (1u << MCU_IRQ_PIN)|
    (1u << I2C_SLAVE_SDA_PIN)|
    (1u << I2C_SLAVE_SCL_PIN);

static uint32_t old_inputs, new_inputs, changed;

// Our handler is called from the I2C ISR, so it must complete quickly. Blocking calls /
// printing to stdio may interfere with interrupt handling.
static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {

    switch (event) {
    case I2C_SLAVE_RECEIVE: // master has written some data - goes to the output packet
            output_context.mem[output_context.mem_address] = i2c_read_byte(i2c);
            output_context.mem_address++;
            mem_address_written++;
        break;
    case I2C_SLAVE_REQUEST: // master is requesting data - comes from the input packet
          // load from memory
          i2c_write_byte(i2c, input_context.mem[input_context.mem_address]);
          input_context.mem_address++;
        break;
    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
            mem_address_written = 0;
            input_context.mem_address = 0;
            output_context.mem_address = 0;
        break;
    default:
        break;
    }
}

static void setup_slave() {
    gpio_init(I2C_SLAVE_SDA_PIN);
    gpio_set_function(I2C_SLAVE_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SLAVE_SDA_PIN);

    gpio_init(I2C_SLAVE_SCL_PIN);
    gpio_set_function(I2C_SLAVE_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SLAVE_SCL_PIN);

     i2c_init(i2c0, I2C_BAUDRATE);
    // configure I2C0 for slave mode
    i2c_slave_init(i2c0, I2C_SLAVE_ADDRESS, &i2c_slave_handler);
}

void i2c_task (void){
    
    // Get current state of pins
    uint32_t comm_pin_values = gpio_get_all() & comm_pin_mask;
    // Apply new values with mask and preserve existing pin states    
    gpio_put_all((outputpacket.value & ~comm_pin_mask) |comm_pin_values);

    // Mask data resides in packet from host
    inputpacket.probe_irq_mask = outputpacket.probe_irq_mask;
    inputpacket.mcu_irq_mask = outputpacket.mcu_irq_mask & ~inputpacket.probe_irq_mask; //remove probe_pins from mcu_irq_mask

    // Read the pin values (inputs and outputs)
    uint32_t getval = gpio_get_all();
    inputpacket.value = getval;

    // Do the necessary math operations to combine pins and set the irq signals accordingly
    static bool alarm_or_input = false;
    static bool probe_or_tool = false;

    new_inputs = (inputpacket.value & input_pin_mask);
    changed = (new_inputs ^ old_inputs);
    old_inputs = new_inputs;

    // MCU interrupt is toggle (stateless)
    if (changed & inputpacket.mcu_irq_mask)
        alarm_or_input ^= 1;
    
    // Probe interrupt tracks active pin state (assumes only one pin is active in irq mask)
    probe_or_tool = (new_inputs & (uint32_t)inputpacket.probe_irq_mask) != 0;

    //IRQ pins are rising/falling (all inversion handled by host)
    gpio_put(MCU_PRB_PIN, probe_or_tool);
    gpio_put(MCU_IRQ_PIN, alarm_or_input);

    #if 0 //testing code

    printf("probe_or_value ");
    // For multiple bytes
    printf("%d", probe_or_value);
    printf("\r\n");

    printf("any_signal_active ");
    // For multiple bytes
    printf("%d", any_signal_active);
    printf("\r\n");

    printf("inputpacket.value ");
    // For multiple bytes
    printf("%d", inputpacket.value);
    printf("\r\n");

    printf("outputpacket.value ");
    // For multiple bytes
    printf("%d", outputpacket.value);
    printf("\r\n");

    sleep_ms(1500);
    #endif
}

void init_i2c_responder (void){

  //Serial.printf("Setup GPIOS\r\n");
  inputpacket.value = 0;
  inputpacket.probe_irq_mask = 0;
  inputpacket.mcu_irq_mask = 0;

  outputpacket.value = 0;
  outputpacket.probe_irq_mask = 0;
  outputpacket.mcu_irq_mask = 0;    

  //set up gpio output pins  
  gpio_init(MCU_IRQ_PIN);
  gpio_put(MCU_IRQ_PIN, false);
  gpio_set_dir(MCU_IRQ_PIN, GPIO_OUT);

  gpio_init(MCU_PRB_PIN);
  gpio_put(MCU_PRB_PIN, false);
  gpio_set_dir(MCU_PRB_PIN, GPIO_OUT);

  gpio_init(SPINEN_PIN);
  gpio_put(SPINEN_PIN, false);
  gpio_set_dir(SPINEN_PIN, GPIO_OUT);

  gpio_init(SPINDR_PIN);
  gpio_put(SPINDR_PIN, false);
  gpio_set_dir(SPINDR_PIN, GPIO_OUT);

  gpio_init(MIST_PIN);
  gpio_put(MIST_PIN, false);
  gpio_set_dir(MIST_PIN, GPIO_OUT);

  gpio_init(COOL_PIN);
  gpio_put(COOL_PIN, false);
  gpio_set_dir(COOL_PIN, GPIO_OUT);

  gpio_init(DISABLEX_PIN);
  gpio_put(DISABLEX_PIN, false);
  gpio_set_dir(DISABLEX_PIN, GPIO_OUT);

  gpio_init(DISABLEY_PIN);
  gpio_put(DISABLEY_PIN, false);
  gpio_set_dir(DISABLEY_PIN, GPIO_OUT);

  gpio_init(DISABLEZ_PIN);
  gpio_put(DISABLEZ_PIN, false);
  gpio_set_dir(DISABLEZ_PIN, GPIO_OUT);

  gpio_init(DISABLEA_PIN);
  gpio_put(DISABLEA_PIN, false);
  gpio_set_dir(DISABLEA_PIN, GPIO_OUT);

  gpio_init(DISABLEB_PIN);
  gpio_put(DISABLEB_PIN, false);
  gpio_set_dir(DISABLEB_PIN, GPIO_OUT);

  gpio_init(DISABLEC_PIN);
  gpio_put(DISABLEC_PIN, false);
  gpio_set_dir(DISABLEC_PIN, GPIO_OUT);

  gpio_init(AUXOUT0_PIN);
  gpio_put(AUXOUT0_PIN, 0);
  gpio_set_dir(AUXOUT0_PIN, GPIO_OUT);

  gpio_init(AUXOUT1_PIN);
  gpio_put(AUXOUT1_PIN, false);
  gpio_set_dir(AUXOUT1_PIN, GPIO_OUT);

  gpio_init(AUXOUT2_PIN);
  gpio_put(AUXOUT2_PIN, false);
  gpio_set_dir(AUXOUT2_PIN, GPIO_OUT);

  gpio_init(AUXOUT3_PIN);
  gpio_put(AUXOUT3_PIN, false);
  gpio_set_dir(AUXOUT3_PIN, GPIO_OUT);

  gpio_init(AUXOUT4_PIN);
  gpio_put(AUXOUT4_PIN, false);
  gpio_set_dir(AUXOUT4_PIN, GPIO_OUT);

  gpio_init(AUXOUT5_PIN);
  gpio_put(AUXOUT5_PIN, false);
  gpio_set_dir(AUXOUT5_PIN, GPIO_OUT);

  gpio_init(AUXOUT6_PIN);
  gpio_put(AUXOUT6_PIN, false);
  gpio_set_dir(AUXOUT6_PIN, GPIO_OUT);

  gpio_init(AUXOUT7_PIN);
  gpio_put(AUXOUT7_PIN, false);
  gpio_set_dir(AUXOUT7_PIN, GPIO_OUT);

  //set up gpio input pins
  gpio_init(TOOL_PIN);
  gpio_put(TOOL_PIN, true);
  gpio_set_dir(TOOL_PIN, GPIO_IN);

  gpio_init(PROBE_PIN);
  gpio_put(PROBE_PIN, true);
  gpio_set_dir(PROBE_PIN, GPIO_IN); 

  gpio_init(ALARMX_PIN);
  gpio_put(ALARMX_PIN, true);
  gpio_set_dir(ALARMX_PIN, GPIO_IN); 

  gpio_init(ALARMY_PIN);
  gpio_put(ALARMY_PIN, true);
  gpio_set_dir(ALARMY_PIN, GPIO_IN); 

  gpio_init(ALARMZ_PIN);
  gpio_put(ALARMZ_PIN, true);
  gpio_set_dir(ALARMZ_PIN, GPIO_IN); 

  gpio_init(ALARMA_PIN);
  gpio_put(ALARMA_PIN, true);
  gpio_set_dir(ALARMA_PIN, GPIO_IN); 

  gpio_init(ALARMB_PIN);
  gpio_put(ALARMB_PIN, true);
  gpio_set_dir(ALARMB_PIN, GPIO_IN); 

  gpio_init(ALARMC_PIN);
  gpio_put(ALARMC_PIN, true);
  gpio_set_dir(ALARMC_PIN, GPIO_IN);             

  // Setup I2C0 as slave (peripheral)
  //Serial.printf("Setup I2C\r\n");
  setup_slave();
  sleep_ms(5);

}