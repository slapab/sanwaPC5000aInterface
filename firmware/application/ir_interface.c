#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "ir_interface.h"
#include "systick_local.h"

#define GPIO_PORT    GPIOB
#define GPIO_DATA_IN GPIO4
#define GPIO_DATA_CLK GPIO3

static void nops_wait(void);

void ir_itf_init(void) {
    rcc_periph_clock_enable(RCC_GPIOB);

    // configure input pin, assume external pull-down or pull-up
    gpio_set_mode(GPIO_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO_DATA_IN);
    // configure clk pin
    gpio_set_mode(GPIO_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO_DATA_CLK);
    gpio_clear(GPIO_PORT, GPIO_DATA_CLK);
}


bool ir_itf_read_blocking(uint8_t* const buffer, const size_t len) {
    bool retval = false;

    if ((NULL != buffer) && (len >= IR_DATA_LEN)) {
    do {
        // preconditions -> IR receiver must giving at this moment '0'
        if (0 != gpio_get(GPIO_PORT, GPIO_DATA_IN)) {
            break;
        }

        // generate 10ms impulse on IR LED
        gpio_clear(GPIO_PORT, GPIO_DATA_CLK);
        asm("nop"); asm("nop"); asm("nop");
        gpio_set(GPIO_PORT, GPIO_DATA_CLK);
        st_delay_ms(11);
        gpio_clear(GPIO_PORT, GPIO_DATA_CLK);

        // wait for '1' from dmm but not more than 200ms
        systick_t tpStart = st_get_ticks();
        while(0 == gpio_get(GPIO_PORT, GPIO_DATA_IN) && st_get_time_duration(tpStart) <= 200/*ms*/) {;}
        // check if DMM send response not timeout
        if (0 == gpio_get(GPIO_PORT, GPIO_DATA_IN)) {
            break;
        }

        // start generating 128 impulses on data clock and read data on falling edge
        for (int byteNo = 0; byteNo < IR_DATA_LEN; ++byteNo) {
            for (int bitNo = 0; bitNo < 8; ++bitNo) {
                gpio_set(GPIO_PORT, GPIO_DATA_CLK);
                //nops_wait();nops_wait();nops_wait();nops_wait();
                st_delay_ms(1);

                gpio_clear(GPIO_PORT, GPIO_DATA_CLK);
                uint16_t bitVal = gpio_get(GPIO_PORT, GPIO_DATA_IN); // 0 or 1
                // insert just read bit into right place of buffer
                // this is branching less replacement for: if(bitVal == 1) buffer[byteNo] |= (1<<bitNo); else buffer[byteNo] &= ~(1<<bitNo);
                buffer[byteNo] ^= (-bitVal ^ buffer[byteNo]) & (1 << bitNo);

//                nops_wait();nops_wait();nops_wait();nops_wait();
                st_delay_ms(1);
            }
        }
        retval = true;
    } while (0);
    } // checking function args

    return retval;
}


static void nops_wait(void) {
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
}
