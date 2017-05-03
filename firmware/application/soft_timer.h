#ifndef SOFT_TIMER_H_
#define SOFT_TIMER_H_
#include <stdbool.h>
#include <signal.h>
#include <stdint.h>
#include "systick_local.h"

/**
 * @file Software timer.
 *
 * @note This is the software timer, and is doesn't guarantee precision timing. It only guarantees that call callback
 * function will be called on timer's timed out event not earlier than specified ticks.
 *
 */


/// Type of callback function called when given software timer is timed out.
typedef void (*soft_timer_callback)(void);

/**
 * Possible types of software timer.
 */
typedef enum {
    SOFT_TIMER_CONTINUOUS,
    SOFT_TIMER_SINGLE_SHOT
} soft_timer_type;

/**
 * Descriptor of software timer.
 */
typedef struct {
    /// Hardware counter value set during timer's start event.
    uint32_t            last_count;
    /// Ticks to timer's timed out event.
    uint32_t            time_out_ticks;
    /// The callback function which will be called when timer was timeout.
    soft_timer_callback callback;
    /// Type of timer, see \ref soft_timer_type.
    soft_timer_type     type;
    /// If set to non 0 value then timer will be deleted from the pool if registered. It can be set also inside the
    /// interrupt routine
    sig_atomic_t        terminating_req;
    /// Is set to true if timer was timed out.
    bool                is_timed_out;
} soft_timer_descr;


/**
 * Registers inside the pool and starts single shot timer. If callback function was specified it will be called if
 * timer times out then the timer will be removed from the pool.
 *
 * @param stim[in]  software timer descriptor to register and start.
 * @param time_out_ticks[in] how many ticks must elapsed until timeout event occur.
 * @param callback[in]  callback function called when timeout event occurs.
 * @return true if software timer was registered and started successfully, false otherwise.
 */
bool soft_timer_start_one_shot(soft_timer_descr* const stim, const systick_t time_out_ticks, soft_timer_callback callback);

/**
 * Polls the registered software timers and checks their state and calls callback function if necessary.
 * It must be called periodically.
 */
void soft_timer_poll(void);

#endif //SOFT_TIMER_H_
