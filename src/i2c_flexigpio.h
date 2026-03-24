#ifndef __I2C_FLEXIGPIO_H__
#define __I2C_FLEXIGPIO_H__

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"

#include "pico/time.h"
#include "pico/stdio.h"

#include "i2c_fifo.h"
#include "i2c_slave.h"

#include "i2c_flexigpio.h"

#define PROTOCOL_VERSION 1
#define I2C_TIMEOUT_VALUE 5

//flash defines
#define FLASH_TARGET_OFFSET (256 * 1024)
//extern const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);
extern const uint8_t *flash_target_contents;

const uint8_t MCU_IRQ_PIN       = 2;
const uint8_t MCU_PRB_PIN       = 15;

const uint8_t TOOL_PIN          = 3;
const uint8_t PROBE_PIN         = 4;

const uint8_t ALARMX_PIN        = 5;
const uint8_t ALARMY_PIN        = 6;
const uint8_t ALARMZ_PIN        = 7;
const uint8_t ALARMA_PIN        = 8;
const uint8_t ALARMB_PIN        = 9;
const uint8_t ALARMC_PIN        = 10;

const uint8_t SPINEN_PIN        = 11;
const uint8_t SPINDR_PIN        = 12;

const uint8_t MIST_PIN          = 13;
const uint8_t COOL_PIN          = 14;

const uint8_t DISABLEX_PIN        = 29;
const uint8_t DISABLEY_PIN        = 28;
const uint8_t DISABLEZ_PIN        = 27;
const uint8_t DISABLEA_PIN        = 26;
const uint8_t DISABLEB_PIN        = 25;
const uint8_t DISABLEC_PIN        = 24;

const uint8_t AUXOUT0_PIN         = 23;
const uint8_t AUXOUT1_PIN         = 22;
const uint8_t AUXOUT2_PIN         = 21;
const uint8_t AUXOUT3_PIN         = 20;
const uint8_t AUXOUT4_PIN         = 19;
const uint8_t AUXOUT5_PIN         = 18;
const uint8_t AUXOUT6_PIN         = 17;
const uint8_t AUXOUT7_PIN         = 16;

//contains data from the host (output set values, polarity mask).
typedef struct __attribute__((packed)) {
    uint32_t value;
    uint32_t direction_mask;    
    uint32_t polarity_mask;
    uint32_t enable_mask;
} output_packet_t;

//contains data sent to the host (pin values).
typedef struct __attribute__((packed)) {
    uint32_t value;
    uint32_t direction_mask;    
    uint32_t polarity_mask;
    uint32_t enable_mask; //allows to mask inputs.
} input_packet_t;

typedef struct
{
    uint8_t mem[128];
    uint8_t mem_address;
    uint8_t mem_address_written;
} status_context_t;

extern status_context_t input_context, output_context;

extern input_packet_t &inputpacket;
extern output_packet_t &outputpacket;

extern uint8_t mem_address;
extern uint8_t mem_address_written;

extern int command_error;

//device specific variables

void init_i2c_responder (void);
void i2c_task(void);

#if defined(_LINUX_) && defined(__cplusplus)
}
#endif // _LINUX_

#endif // __I2C_FLEXIGPIO_H__

