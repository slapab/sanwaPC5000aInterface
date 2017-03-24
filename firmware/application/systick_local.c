#include "systick_local.h"

static systick_t counter;

void sys_tick_handler(void) {
    ++counter;
}

systick_t st_get_ticks(void) {
    return counter;
}

uint32_t st_get_time_duration(const systick_t start_time_point) {
    return (uint32_t)((uint32_t)counter - (uint32_t)start_time_point);
}



