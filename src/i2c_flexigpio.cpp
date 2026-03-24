
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

#if 0
uint8_t keypad_sendcount (bool clearpin) {
  //maybe use key_pressed variable to avoid spamming?
  int timeout = I2C_TIMEOUT_VALUE;

  command_error = 0;

  //make sure no transmission is active
  //while (mem_address_written != 0 && mem_address>0);
  while (mem_address_written != 0 && status_context.mem_address>0 &&count_context.mem_address>0);

    //context.mem[0] = character;
    count_context.mem_address = 0;
    gpio_put(KPSTR_PIN, false);
    sleep_us(200);
    while (count_context.mem_address == 0 && timeout){
      sleep_us(200);      
      timeout = timeout - 1;}

    if(!timeout)
      command_error = 1;
    //sleep_ms(2);
    if (clearpin){
      sleep_us(10);
      gpio_put(KPSTR_PIN, true);
    }

  return true;
};

#endif

void i2c_task (void){
    
    //Set the outputs
    const uint32_t mask = ~((1 << MCU_PRB_PIN) | (1 << MCU_IRQ_PIN) | (1 << I2C_SLAVE_SDA_PIN) | (1 << I2C_SLAVE_SCL_PIN));
    // Get current state of pins
    uint32_t mask_values = gpio_get_all() & ~mask;
    // Apply new values with mask and preserve existing pin states    
    gpio_put_all((outputpacket.value & mask) | mask_values);
    //gpio_put_all(outputpacket.value);

    //polarity mask resides in the data read from the host.  Polarity is only used for the inputs (for now?)
    // inputpacket.polarity_mask = outputpacket.polarity_mask;
    // inputpacket.enable_mask = outputpacket.enable_mask;
    // inputpacket.direction_mask = outputpacket.direction_mask;

    //Read the pin values (inputs and outputs)
    uint32_t getval = gpio_get_all();
    // uint32_t masked = getval & inputpacket.enable_mask;
    // masked ^= (inputpacket.polarity_mask & inputpacket.enable_mask);
    inputpacket.value = getval;

    //Finally, do the necessary math operations on the motor and probe pins to combine then and set the outputs accordingly.

    bool any_alarm_active = ((~inputpacket.value) &
     ((1u << ALARMX_PIN) |
      (1u << ALARMY_PIN) | 
      (1u << ALARMZ_PIN) | 
      (1u << ALARMA_PIN) | 
      (1u << ALARMB_PIN) | 
      (1u << ALARMC_PIN))) != 0;

    bool probe_or_value = (inputpacket.value & (1 << TOOL_PIN)) || (inputpacket.value & (1 << PROBE_PIN)); //or
    //bool probe_or_value = !!(inputpacket.value & (1 << TOOL_PIN)) ^ !!(inputpacket.value & (1 << PROBE_PIN)); //xor

    //IRQ pins are active high.
    probe_or_value = probe_or_value;
    any_alarm_active = any_alarm_active;

    //apply inversion to these outputs.
    if (outputpacket.polarity_mask & (1 << MCU_PRB_PIN))
      gpio_put(MCU_PRB_PIN, !probe_or_value);
    else
      gpio_put(MCU_PRB_PIN, probe_or_value);


    gpio_put(MCU_IRQ_PIN, any_alarm_active);

    #if 0 //testing code

    printf("alarmx / tool ");
    // For multiple bytes
    printf("%d", (getval & (1 << ALARMX_PIN)) != 0);
    printf("\r\n");

     printf("alarmy / probe ");
    // For multiple bytes
    printf("%d", (getval & (1 << ALARMY_PIN)) != 0);
    printf("\r\n");

    // printf("masked ");
    // // Print each bit from MSB to LSB
    // for (int i = sizeof(masked) * 8 - 1; i >= 0; i--) {
    //     printf("%d", (masked >> i) & 1);
    // }
    // printf("\r\n");

    // printf("inputpacket.polarity_mask ");
    // // For multiple bytes
    // printf("%d", inputpacket.polarity_mask);
    // printf("\r\n");  

    // printf("inputpacket.enable_mask ");
    // // For multiple bytes
    // printf("%d", inputpacket.enable_mask);
    // printf("\r\n");  

    //Serial.printf("inputpacket.direction_mask ");
    // For multiple bytes
    //Serial.printf("%d", inputpacket.direction_mask);
    //Serial.printf("\r\n");  

    printf("any_alarm_active ");
    // For multiple bytes
    printf("%d", any_alarm_active);
    printf("\r\n");

    // printf("probe_or_value ");
    // // For multiple bytes
    // printf("%d", probe_or_value);
    // printf("\r\n");

    // printf("inputpacket.value ");
    // // For multiple bytes
    // printf("%d", inputpacket.value);
    // printf("\r\n");

    // printf("outputpacket.value ");
    // // For multiple bytes
    // printf("%d", outputpacket.value);
    // printf("\r\n");

    sleep_ms(500);
    #endif

}

void init_i2c_responder (void){

  //Serial.printf("Setup GPIOS\r\n");
  inputpacket.value = 0;
  inputpacket.polarity_mask = 0;
  inputpacket.enable_mask = 0;

  outputpacket.value = 0;
  outputpacket.polarity_mask = 0;
  outputpacket.enable_mask = 0;    


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