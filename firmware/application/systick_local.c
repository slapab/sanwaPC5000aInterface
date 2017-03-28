#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include "systick_local.h"


static systick_t counter;

void sys_tick_handler(void) {
    ++counter;
}

bool st_init(const uint32_t systick_freq, const uint32_t ahb_freq) {
    bool retval = systick_set_frequency(systick_freq, ahb_freq);
    systick_interrupt_enable();
    /* Start counting. */
    systick_counter_enable();
    return retval;
}

systick_t st_get_ticks(void) {
    return counter;
}

uint32_t st_get_time_duration(const systick_t start_time_point) {
    return (uint32_t)((uint32_t)counter - (uint32_t)start_time_point);
}

void st_delay_ms(uint32_t delay) {
    while (delay--) {;}
}
