
#include <threads.h>
#include <time.h>

#include "platform.h"

typedef struct timespec time_type;

static time_type current_time(void)
{
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == 0) {
        exit(EXIT_FAILURE);
    }
    return ts;
}

static unsigned time_diff(time_type t0, time_type t1)
{
    t0.tv_sec -= t1.tv_sec;
    if (t1.tv_nsec > t0.tv_nsec) {
        t0.tv_sec--;
        t0.tv_nsec += 1000000000;
    }
    t0.tv_nsec -= t1.tv_nsec;
    return t0.tv_sec * 100 + t0.tv_nsec / 10000000;
}

static mtx_t timer_mtx;

static int timer_trylock(void)
{
    switch (mtx_trylock(&timer_mtx)) {
        case thrd_success: return 0;
        case thrd_busy: return -1;
        default: exit(EXIT_FAILURE);
    }
}

static void timer_unlock(void)
{
    if (mtx_unlock(&timer_mtx) != thrd_success) {
        exit(EXIT_FAILURE);
    }
}

struct thread_wrap {
    bool used;
    thrd_t thread;
    void (*entry)(void *);
    void *arg;
    bool end_flag;
};

static mtx_t thread_creation_mtx;

static void xlock(void)
{
    if (mtx_lock(&thread_creation_mtx) != thrd_success) {
        exit(EXIT_FAILURE);
    }
}

static void xunlock(void)
{
    if (mtx_unlock(&thread_creation_mtx) != thrd_success) {
        exit(EXIT_FAILURE);
    }
}

void init_threading(void)
{
    if (mtx_init(&thread_creation_mtx, mtx_plain) != thrd_success) {
        exit(EXIT_FAILURE);
    }
    if (mtx_init(&timer_mtx, mtx_plain) != thrd_success) {
        exit(EXIT_FAILURE);
    }
}

static thread_local struct thread_wrap *this_thread;

static volatile struct thread_wrap threads[128];

static bool is_valid_thread_index(int i)
{
    return (i > 0 && i < ARRAY_LENGTH(threads));
}

void thread_exit(void)
{
    this_thread->used = false;
    thrd_exit(0);
}

void search_thread_cancel_point(void)
{
    if (this_thread->end_flag) {
        thread_exit();
    }
    check_timeout();
}

static int thread_wrapper(void *arg)
{
    this_thread = (struct thread_wrap *)arg;

    this_thread->entry(this_thread->arg);
    return 0;
}

void thread_create(thread_pointer t, void (*entry)(void *), void *arg)
{
    xlock();
    
    int *thread = (int*)t;
    for (int i = 1; i < ARRAY_LENGTH(threads); ++i) {
        volatile struct thread_wrap *tw = threads + i;
        if (!tw->used) {
            thrd_t *thrd = (thrd_t*)&(tw->thread);

            tw->used = true;
            tw->end_flag = false;
            tw->arg = arg;
            tw->entry = entry;
            if (thrd_create(thrd, thread_wrapper, (void*)tw) != thrd_success) {
                exit(EXIT_FAILURE);
            }
            *thread = i;
            xunlock();
            return;
        }
    }
    exit(EXIT_FAILURE);
}

static void wait_thread(int *thread, bool set_end_flag)
{
    if (!is_valid_thread_index(*thread)) return;
    xlock();
    if (threads[*thread].used) {
        if (set_end_flag) {
            threads[*thread].end_flag = true;
        }
        if (thrd_join(threads[*thread].thread, NULL) != thrd_success) {
            exit(EXIT_FAILURE);
        }
    }
    *thread = 0;
    xunlock();
}

void thread_kill(thread_pointer thread)
{
    wait_thread((int*)thread, true);
}

void thread_join(thread_pointer thread)
{
    wait_thread((int*)thread, false);
}

thread_pointer thread_allocate(void)
{
    return malloc(sizeof(int));
}

