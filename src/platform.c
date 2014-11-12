
#include <stdbool.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

uintmax_t get_big_endian_num(int size, const unsigned char str[size])
{
    uintmax_t value = 0;

    while (size > 0) {
        value = (value << 8) + *str;
        ++str;
        --size;
    }
    return value;
}

static void check_timeout(void);

#if defined(POSIX_BUILD)
#   include "platform_posix.inc"
#elif defined(HAS_C11_THREADS)
#   include "platform_std_c11.inc"
#elif defined(__POCC__)
#   include "platform_pelles.inc"
#else
#   error "I don't know how to compile this. That makes me a sad panda."
#endif


static time_type timer_started;
static bool is_timer_set = false;
static unsigned max_time_allowed;
static void (*timer_cb)(void *);
static void *timer_cb_arg;

void set_timer_cb(void (*cb)(void *), void *arg)
{
    timer_cb = cb;
    timer_cb_arg = arg;
}

void set_timer(unsigned centi_seconds)
{
    timer_started = current_time();
    max_time_allowed = centi_seconds;
    is_timer_set = true;
}

unsigned get_timer(void)
{
    if (is_timer_set) {
        return time_diff(current_time(), timer_started);
    }
    else {
        return 0;
    }
}

void end_timer(void)
{
    if (timer_trylock() == 0) {
        is_timer_set = false;
        if (timer_cb != NULL) {
            timer_cb(timer_cb_arg);
            timer_cb = NULL;
        }
        timer_unlock();
    }
}

static void check_timeout(void)
{
    if (!is_timer_set) return;
    if (time_diff(current_time(), timer_started) >= max_time_allowed) {
        end_timer();
        thread_exit();
    }
}

void cancel_timer(void)
{
    is_timer_set = false;
}

