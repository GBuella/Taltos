
#include <windows.h>
#include <stdint.h>

#include "platform.h"

thread_pointer thread_allocate(void)
{
    return malloc(sizeof(HANDLE));
}

void search_thread_cancel_point(void)
{
    check_timeout();
}

struct call_data {
    void (*entry)(void *);
    void *arg;
};

HANDLE *thread_wrapper(void *arg)
{
    struct call_data data;

    memcpy(&data, arg, sizeof data);
    free(arg);
    data.entry(data.arg);
    return NULL;
}

void thread_create(thread_pointer thread, void (*entry)(void *), void *arg)
{
    HANDLE *thread = (HANDLE*)t;
    struct call_data *cd = malloc(sizeof *cd);

    if (cd == NULL) {
        exit(EXIT_FAILURE);
    }
    cd->entry = entry;
    cd->arg = arg;
    *thread = CreateThread(NULL, 0,
            (LPTHREAD_START_ROUTINE) thread_wrapper, cd, 0, NULL);
    if (*thread == NULL) {
        exit(EXIT_FAILURE);
    }
}

void thread_exit(void)
{
    ExitThread(0);
}

void thread_kill(thread_pointer t)
{
    HANDLE *thread = (HANDLE*)t;

    if (*thread) {
        TerminateThread(*thread, 0);
        *thread = NULL;
    }
}

void thread_join(thread_pointer t)
{
    HANDLE *thread = (HANDLE*)t;

    WaitForSingleObject(thread);
    *thread = NULL;
}

void init_threading(void) { }

