
#include "timers.h"

#ifdef WITH_MACH_TIMER

/* Probably this code only works on Intel based Macs */

#include <mach/mach_time.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "str_util.h"

static uint64_t mach_timers_sum[TIMER_COUNT];
static uint64_t mach_timers_count[TIMER_COUNT];
static uint64_t mach_timers_started[TIMER_COUNT];

void timers_reset(void)
{
    memset(mach_timers_sum, 0, sizeof mach_timers_sum);
    memset(mach_timers_count, 0, sizeof mach_timers_count);
    memset(mach_timers_started, 0, sizeof mach_timers_started);
}

void timer_start(timer_entry entry)
{
    mach_timers_started[entry] = mach_absolute_time();
}

void timer_stop(timer_entry entry)
{
    //(void)getpid();
    mach_timers_sum[entry] +=
        mach_absolute_time() - mach_timers_started[entry];
    mach_timers_count[entry]++;
}

static uint64_t convert_to_ns(uint64_t value)
{
    mach_timebase_info_data_t s_timebase_info;

    (void)mach_timebase_info(&s_timebase_info);
    return value * s_timebase_info.numer / s_timebase_info.denom;
      /* ...and hope to have no overflow... */
}

uint64_t get_timer_sum(timer_entry entry)
{
    return convert_to_ns(mach_timers_sum[entry]);
}

uint64_t get_timer_count(timer_entry entry)
{
    return mach_timers_count[entry];
}

void timers_print(bool use_unicode)
{
    static const char *timer_name[TIMER_COUNT];

    timer_name[TIMER_EVAL] = "static evaluation";
    timer_name[TIMER_MOVE_GEN] = "move generator";
    timer_name[TIMER_MOVE_SELECT_NEXT] = "move ordering";

    for (size_t i = 0; i < TIMER_COUNT; ++i) {
        if (mach_timers_count[i] > 0) {
            uint64_t sum = get_timer_sum(i);
            printf("timer - %s: count=%" PRIu64,
                   timer_name[i], mach_timers_count[i]);
            printf(" sum=");
            print_nice_ns(sum, use_unicode);
            printf(" avg=");
            print_nice_ns(sum / mach_timers_count[i], use_unicode);
            printf("\n");
        }
        else {
            printf("timer - %s: N/A\n", timer_name[i]);
        }
    }


}

#endif
