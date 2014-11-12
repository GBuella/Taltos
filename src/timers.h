
#ifndef TIMERS_H
#define TIMERS_H

#include <stdbool.h>
#include <stdint.h>

enum enum_timer_entry {
    TIMER_EVAL,
    TIMER_MOVE_GEN,
    TIMER_MOVE_SELECT_NEXT,
    TIMER_COUNT
};

typedef enum enum_timer_entry timer_entry;

#ifdef WITH_MACH_TIMER

void timers_reset(void);
void timer_start(timer_entry);
void timer_stop(timer_entry);
uint64_t get_timer_sum(timer_entry);
uint64_t get_timer_count(timer_entry);
void timers_print(bool use_unicode);

#else

#define TIMER_COUNT 0

static inline void timers_reset(void) {}

static inline void timer_start(timer_entry e)
{
    (void)e;
}

static inline void timer_stop(timer_entry e)
{
    (void)e;
}

static inline uint64_t get_timer_sum(timer_entry e)
{
    (void)e;
    return 0;
}

static inline uint64_t get_timer_count(timer_entry e)
{
    (void)e;
    return 0;
}

static inline void timers_print(bool use_unicode)
{
    (void)use_unicode;
}

#endif

#endif

