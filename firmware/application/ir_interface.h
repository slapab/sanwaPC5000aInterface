#ifndef IR_INTERFACE_H_
#define IR_INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/// Length of buffer (in bytes) required to store data read with IR interface
#define IR_DATA_LEN 16

/**
 * Initializes I/O
 */
void ir_itf_init_blocking(void);

/**
 * Reads data from DMM - this is blocking operation.
 *
 * @param [in,out] buff pointer to memory where read data will be stored
 * @param [in] len length of providing buffer
 * @return true if read operations was successful, false otherwise
 */
bool ir_itf_read_blocking(uint8_t* const buffer, const size_t len);


/// todo comment
void ir_itf_init_nb(void);

/// todo comment
void ir_itf_start_read_nb(uint8_t* const buffer, const size_t len);

int ir_itf_get_status(void);
/**
 * Starts reading data from DMM - this is non blocking blocking operations. It will use SPI peripheral and DMA to
 * perform ...
 */
//bool ir_itf_start_read(uint8_t* const buffer, const size_t len /*timer*/);

#endif //IR_INTERFACE_H_
