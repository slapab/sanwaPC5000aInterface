/**
 * @file Implementation of software timer.
 *
 * @note This is the software timer, and is doesn't guarantee precision timing. It only guarantees that call callback
 * function will be called on timer's timed out event not earlier than specified ticks.
 *
 */

#include "soft_timer.h"


#define SOFT_TIMERS_POOL_SIZE 16
static soft_timer_descr* soft_timers_pool[SOFT_TIMERS_POOL_SIZE];

bool soft_timer_start_one_shot(soft_timer_descr* const stim, const systick_t time_out_ticks, soft_timer_callback callback) {
    bool retval = false;
    if (NULL != stim) {

        int avaiableIdx = -1;
        // find free space in pool or the same timer if it already inserted
        for (int i = 0; i < SOFT_TIMERS_POOL_SIZE; ++i) {
            if ((NULL == soft_timers_pool[i]) || (stim == soft_timers_pool[i])) {
                avaiableIdx = i;
                break;
            }
        }

        if (avaiableIdx >= 0) {
            // found free space or timer is already inside the pool. In both cases set values in timer
            stim->callback = callback;
            stim->is_timed_out = false;
            stim->time_out_ticks = time_out_ticks;
            stim->type = SOFT_TIMER_SINGLE_SHOT;
            stim->terminating_req = 0;
            stim->last_count = (uint32_t)st_get_ticks();
            retval = true;

            // store software timer inside the pool
            soft_timers_pool[avaiableIdx] = stim;
        }
    }

    return retval;
}


void soft_timer_poll(void) {
    for (int i = 0; i < SOFT_TIMERS_POOL_SIZE; ++i) {
        soft_timer_descr* const tim = soft_timers_pool[i];
        if (NULL != tim) {

            if (0 != tim->terminating_req) {
                // request to terminate the software timer is set -> delete it from the pool
                tim->is_timed_out = true;
                soft_timers_pool[i] = NULL;
            } else if ((st_get_time_duration(tim->last_count) >= tim->time_out_ticks) || (true == tim->is_timed_out)) {
                // timed out
                if (SOFT_TIMER_SINGLE_SHOT == tim->type) {
                    tim->is_timed_out = true;
                    // check if callback is set and also check again terminating request, because it can be set inside
                    // the interrupt routine
                    if ((0 == tim->terminating_req) && (NULL != tim->callback)) {
                        tim->callback();
                    }
                    // remove from pool
                    soft_timers_pool[i] = NULL;
                } else {
                    // continuous timer
                    if (NULL != tim->callback) {
                        tim->is_timed_out = true;
                        tim->callback();
                    }
                    tim->is_timed_out = false;
                    tim->last_count = (uint32_t)st_get_ticks();
                }
            }
        }
    }
}
