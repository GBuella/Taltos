
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#include "platform.h"

typedef struct timeval time_type;

static time_type current_time(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        exit(EXIT_FAILURE);
    }
    return tv;
}

static unsigned time_diff(time_type t0, time_type t1)
{
    t0.tv_sec -= t1.tv_sec;
    if (t1.tv_usec > t0.tv_usec) {
        t0.tv_sec--;
        t0.tv_usec += 1000000;
    }
    t0.tv_usec -= t1.tv_usec;
    return t0.tv_sec * 100 + t0.tv_usec / 10000;
}

static pthread_mutex_t timer_mtx = PTHREAD_MUTEX_INITIALIZER;

static int timer_trylock(void)
{
    switch (pthread_mutex_trylock(&timer_mtx)) {
        case 0: return 0;
        case EBUSY: return -1;
        default: exit(EXIT_FAILURE);
    }
}

static void timer_unlock(void)
{
    if (pthread_mutex_unlock(&timer_mtx) != 0) {
        exit(EXIT_FAILURE);
    }
}

thread_pointer thread_allocate(void)
{
    pthread_t *p = malloc(sizeof(pthread_t));

    if (p == NULL) {
        return NULL;
    }
    *p = (pthread_t)0;
    return p;
}

void search_thread_cancel_point(void)
{
    pthread_testcancel();
    check_timeout();
}

struct call_data {
    void (*entry)(void *);
    void *arg;
};

void *thread_wrapper(void *arg)
{
    struct call_data data;

    memcpy(&data, arg, sizeof data);
    free(arg);
    data.entry(data.arg);
    return NULL;
}

void thread_create(thread_pointer thread, void (*entry)(void *), void *arg)
{
    struct call_data *cd = malloc(sizeof *cd);

    if (cd == NULL) {
        exit(EXIT_FAILURE);
    }
    cd->entry = entry;
    cd->arg = arg;
    if (pthread_create((pthread_t*)thread, NULL, thread_wrapper, cd) != 0) {
        exit(EXIT_FAILURE);
    }
}

void thread_exit(void)
{
    pthread_exit(NULL);
}

void thread_join(thread_pointer t)
{
    pthread_t *thread = (pthread_t*)t;

    if (thread != NULL && *thread != 0) {
        pthread_join(*thread, NULL);
        *thread = (pthread_t)0;
    }
}

void thread_kill(thread_pointer t)
{
    pthread_t *thread = (pthread_t*)t;

    if (thread == NULL || *thread == 0) {
        return;
    }
    if (pthread_cancel(*thread) == 0) {
        pthread_detach(*thread);
        pthread_join(*thread, NULL);
        *thread = (pthread_t)0;
    }
}

void init_threading(void) { }

