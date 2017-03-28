#ifndef SYSTICK_LOCAL_H_
#define SYSTICK_LOCAL_H_
#include <signal.h> // need type: sig_atomic_t
#include <stdint.h>
#include <stdbool.h>


/// Type that represents current local ticks counted by SysTick handler.
typedef sig_atomic_t systick_t;

/// Type of function that returns local value of counter incremented by SysTick interrupt handler.
typedef systick_t (*st_get_ticks_t)(void);

/**
 * Configures and starts SysTick.
 * @param systick_freq required SysTick frequency
 * @param ahb_freq current frequency of AHB bus
 * @return true if configurations was successful
 */
bool st_init(const uint32_t systick_freq, const uint32_t ahb_freq);

/**
 * Returns local value of counter incremented by SysTick interrupt handler.
 */
systick_t st_get_ticks(void);

uint32_t st_get_time_duration(const systick_t start_time_point);

void st_delay_ms(uint32_t delay);

#endif // SYSTICK_LOCAL_H_
