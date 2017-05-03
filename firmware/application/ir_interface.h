#ifndef IR_INTERFACE_H_
#define IR_INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/// Length of buffer (in bytes) required to store data read with IR interface
#define IR_DATA_BYTES 16


typedef enum {
    IR_ITF_READY = 0,
    IR_ITF_WAITING_FOR_DMM,
    IR_ITF_WORKING,
    IR_ITF_DONE
} ir_itf_state_type;

/**
 * Initializes I/O for blocking data reading transaction.
 *
 * @note DMM requires a few milliseconds of the start impulse and then need to wait of dozens milliseconds until it will
 * be ready to transmit.
 */
void ir_itf_init_blocking(void);

/**
 * Reads data from DMM - this is blocking operation.
 *
 * @param[in,out] buff pointer to memory where read data will be stored
 * @param[in] len length of providing buffer
 * @return true if read operations was successful, false otherwise
 *
 * @note DMM requires a few milliseconds of the start impulse and then need to wait of dozens milliseconds until it will
 * be ready to transmit. Transaction will read 128 bits when period of clock is 2ms.
 */
bool ir_itf_read_blocking(uint8_t* const buffer, const size_t len);

/**
 * Initializes I/O and timer used during non-blocking data transmission from the DMM.
 */
void ir_itf_init_nb(void);

/** todo return type to distinguish if starting was successful
 * Begins (if not busy) non-blocking raw data transmission from the DMM. After calling this function the whole process
 * is performed automatically. To check current transmission state use \ref todo: point to proper function!
 *
 * The reading process consist of few steps:
 * 1. Generating impulse (few milliseconds) to request data from the DMM.
 * 2. When DMM is ready to transmit, turns on its IR LED (exti event).
 * 3. Then the timer configured in PWM mode generates clock cycles to receive 128 bits of data. Reading data is performed
 *    on clock's falling edge.
 *
 * @param[out] buffer   pointer to the buffer where received data from DMM will be stored.
 * @param[in]  len      length of the buffer. Can not be less than \ref IR_DATA_LEN.
 *
 * @note DMM turns on its IR LED when sends '0' bit.
 */
void ir_itf_start_read_nb(uint8_t* const buffer, const size_t len);

// todo something to check status and check if can start reading data from the DMM.
ir_itf_state_type ir_itf_get_status(void);


#endif //IR_INTERFACE_H_
