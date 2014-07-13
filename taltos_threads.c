
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

static void (*timer_cb)(void *);
static void *timer_cb_arg;

#if POSIX_BUILD

#include <sys/time.h>
#include <pthread.h>

#include "taltos_threads.h"

static void alarm_handler(int n UNUSED)
{
	(*timer_cb)(timer_cb_arg);
}

void set_timer(unsigned int csecs)
{
    struct itimerval itimerval;

    itimerval.it_interval.tv_sec = 0;
    itimerval.it_interval.tv_usec = 0;
    itimerval.it_value.tv_sec = csecs / 100;
    itimerval.it_value.tv_usec = csecs % 100 * 10000;
    if (setitimer(ITIMER_REAL, &itimerval, NULL) == -1) {
        exit(EXIT_FAILURE);
    }
}

unsigned get_timer(void)
{
    struct itimerval itimerval;
    unsigned int csecs;

    if (getitimer(ITIMER_REAL, &itimerval)) {
        exit(EXIT_FAILURE);
    }
    csecs = (unsigned int)itimerval.it_value.tv_sec * 100;
    csecs += itimerval.it_value.tv_usec / 10000;
    return csecs;
}

void thread_create(pthread_t *thread, entry_t entry, void *arg)
{
	if (pthread_create(thread, NULL, entry, arg) != 0) {
		exit(EXIT_FAILURE);
	}
}

void thread_exit(void)
{
	pthread_exit(NULL);
}

void thread_join(pthread_t *thread)
{
	if (thread != NULL && *thread != 0) {
		pthread_join(*thread, NULL);
	}
}

void thread_kill(pthread_t *thread)
{
    if (thread == NULL || *thread == 0) {
        return;
    }
    if (pthread_cancel(*thread) == 0) {
        pthread_detach(*thread);
        pthread_join(*thread, NULL);
        *thread = NULL;
    }
}

void cancel_timer(void)
{
	struct itimerval itimerval;

	itimerval.it_interval.tv_sec = 0;
	itimerval.it_interval.tv_usec = 0;
	itimerval.it_value.tv_sec = 0;
	itimerval.it_value.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &itimerval, NULL) == -1) {
		exit(EXIT_FAILURE);
	}
}

#elif WINDOWS_BUILD

#include <windows.h>
#include <stdint.h>

#include "taltos_threads.h"

static HANDLE timer_thread;
static int64_t timer_time_out;

static DWORD timer_handler(void *arg)
{
	HANDLE timer_id;
	LARGE_INTEGER rel_time;
	
	timer_id = CreateWaitableTimer(NULL, TRUE, NULL);
	if (timer_id == NULL) {
		TerminateThread(timer_thread, 0);
		return 0;
	}
	rel_time.QuadPart = -(timer_time_out * 10000000 / 1000);
	if (!SetWaitableTimer(timer_id, &rel_time,
				0, NULL, NULL, FALSE))
	{
		CloseHandle(timer_id);
		TerminateThread(timer_thread, 0);
		return 0;
	}
	if (!WaitForSingleObject(timer_id, INFINITE)) {
		(*timer_cb)(timer_cb_arg);
	}
	CloseHandle(timer_id);
	TerminateThread(timer_thread, 0);
	return 0;
}

void set_timer(unsigned int csecs)
{
	if (timer_thread != NULL) {
		return;
	}
	timer_time_out = csecs * 10;
	thread_create(&timer_thread, (entry_t) timer_handler, NULL);
}

unsigned int get_timer(void)
{
	retur 0;
}

void thread_create(HANDLE *thread, entry_t entry, void *arg)
{
	*thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) entry, arg, 0, NULL);
	if (*thread == NULL) {
		exit(EXIT_FAILURE);
	}
}

void thread_exit(void)
{
	ExitThread(0);
}

void thread_kill(HANDLE *thread)
{
	if (*thread) {
		TerminateThread(*thread, 0);
		*thread = NULL;
	}
}

void cancel_timer(void)
{
	if (timer_thread == NULL) {
		return;
	}
	TerminateThread(timer_thread, 0);
	timer_thread = NULL;
}

void thread_join(HANDLE *thread)
{
	if (timer_thread != NULL) {
		WaitForSingleObject(thread);
	}
}

#endif /* WINDOWS_BUILD */

void set_timer_cb(void (*func)(void *), void *arg)
{
#	if POSIX_BUILD
	signal(SIGALRM, alarm_handler);
#	endif
	timer_cb = func;
	timer_cb_arg = arg;
}

