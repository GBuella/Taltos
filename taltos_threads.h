
#ifndef TALTOS_THREADS_H
#define TALTOS_THREADS_H

#include "macros.h"

#if POSIX_BUILD

#include <pthread.h>

typedef pthread_t thread_t;
typedef void *(*entry_t)(void *arg);
static inline void thread_cancel_point(void)
{
    pthread_testcancel();
}

#elif WINDOWS_BUILD

#include <stdint.h>

typedef uint32_t thread_t;
typedef uint32_t (*entry_t)(void *arg);
static inline void thread_cancel_point(void) {}

#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ > 201100L) \
        && !defined(__STDC_NO_THREADS__)

#include <threads.h>

typedef thrd_t thread_t
typedef thrd_start_t entry_t

#else

#   error "Don't know how to use threads in this environment"

#endif

void thread_create(thread_t *thread, entry_t entry, void *arg);
void thread_exit(void);
void thread_kill(thread_t *thread);
void set_timer(unsigned csecs);
void set_timer_cb(void (*func)(void *), void *arg);
unsigned get_timer(void);
void cancel_timer(void);
void thread_join(thread_t *thread);

#endif
