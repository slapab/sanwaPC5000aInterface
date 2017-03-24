#ifndef SYSTICK_LOCAL_H_
#define SYSTICK_LOCAL_H_
#include <signal.h> // need type: sig_atomic_t
#include <stdint.h>

/// Type that represents current local ticks counted by SysTick handler.
typedef sig_atomic_t systick_t;

/// Type of function that returns local value of counter incremented by SysTick interrupt handler.
typedef systick_t (*st_get_ticks_t)(void);
/**
 * Returns local value of counter incremented by SysTick interrupt handler.
 */
systick_t st_get_ticks(void);

uint32_t st_get_time_duration(const systick_t start_time_point);

void st_delay_ms(uint32_t delay) {
    while (delay--) {;}
}

#endif // SYSTICK_LOCAL_H_
