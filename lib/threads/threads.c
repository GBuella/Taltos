/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright(c) 2012 Marcus Geelnard
 * Copyright(c) 2013-2016 Evan Nemerson
 * Copyright 2017 Gabor Buella
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such,
 *    and must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from
 *    any source distribution.
 */
/*
 * Based on tinycthreads at: https://github.com/tinycthread/tinycthread
 */

#include "threads.h"

#include <stdbool.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

#define USING_WINDOWS_THREADS 1

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define __UNDEF_LEAN_AND_MEAN
#endif

#include <windows.h>

#ifdef __UNDEF_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#undef __UNDEF_LEAN_AND_MEAN
#endif

typedef HANDLE system_thrd_t;
typedef struct {
	union {
		// Critical section handle (used for non-timed mutexes)
		CRITICAL_SECTION cs;

		// Mutex handle (used for timed mutex)
		HANDLE mut;
	} handle;
	bool is_already_locked;
	bool is_recursive;
	bool is_timed;
} system_mtx_t;

typedef struct {
	HANDLE events[2]; // Signal and broadcast event HANDLEs
	unsigned int waiters_count;
	CRITICAL_SECTION waiters_count_lock;
} system_cnd_t;

// indices used with the events array in system_cnd_t
enum {
	condition_event_one,
	condition_event_all
}

static unsigned long long
duration_milliseconds(const struct timespec duration)
{
	unsigned long long milliseconds = duration.tv_sec * 1000;
	milliseconds += duration.tv_nsec / 1000000;
	milliseconds += (((duration.tv_nsec % 1000000) == 0) ? 0 : 1);

	return milliseconds;
}

#else
#define USING_POSIX_THREADS 1
#include <pthread.h>
#include <sched.h>

typedef pthread_mutex_t system_mtx_t;
typedef pthread_t system_thrd_t;
typedef pthread_cond_t system_cnd_t;

#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static system_thrd_t
get_system_thrd_t(thrd_t value)
{
	system_thrd_t system_value;
	memcpy(&system_value, &value, sizeof(system_value));
	return system_value;
}

struct thread_startup_data
{
	thrd_start_t func;
	void *arg;
};

#ifdef USING_POSIX_THREADS
static void* thrd_wrapper(void *arg)
#else
static DWORD WINAPI thrd_wrapper(void *arg)
#endif
{
	struct thread_startup_data data = *((struct thread_startup_data*) arg);
	free(arg);

	int r = data.func(data.arg);
#ifdef USING_POSIX_THREADS
	return (void*)(intptr_t) r;
#else
	return (DWORD) r;
#endif
}

int
thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	struct thread_startup_data *startup_data = malloc(sizeof(startup_data));
	if (startup_data == NULL)
		return thrd_nomem;

	startup_data->func = func;
	startup_data->arg = arg;
	int result = thrd_success;

#ifdef USING_POSIX_THREADS
	pthread_t *store = (pthread_t*) thr;
	if (pthread_create(store, NULL, thrd_wrapper, startup_data) != 0)
		result = thrd_error;
#else
	HANDLE *store = (HANDLE*) thr;
	*store = CreateThread(NULL, 0, thrd_wrapper, (LPVOID) startup_data,
			      0, NULL);
	if (*store == NULL)
		result = thrd_error;
#endif
	if (result == thrd_error)
		free(startup_data);

	return result;
}

thrd_t
thrd_current(void)
{
	thrd_t result;
#ifdef USING_WINDOWS_THREADS
	*((HANDLE WINAPI*) &result) =  GetCurrentThread();
#else
	*((pthread_t*) &result) = pthread_self();
#endif
	return result;
}

int
thrd_detach(thrd_t thr)
{
	system_thrd_t t = get_system_thrd_t(thr);
#ifdef USING_WINDOWS_THREADS
  /* https://stackoverflow.com/questions/12744324/how-to-detach-a-thread-on-windows-c#answer-12746081 */
	return (CloseHandle(t) != 0) ? thrd_success : thrd_error;
#else
	return (pthread_detach(t) == 0) ? thrd_success : thrd_error;
#endif
}

int
thrd_equal(thrd_t thr0, thrd_t thr1)
{
	system_thrd_t t0 = get_system_thrd_t(thr0);
	system_thrd_t t1 = get_system_thrd_t(thr1);
#ifdef USING_WINDOWS_THREADS
	return GetThreadId(t0) == GetThreadId(t1);
#else
	return pthread_equal(t0, t1);
#endif
}

int
thrd_join(thrd_t thr, int *res)
{
	system_thrd_t t = get_system_thrd_t(thr);
#ifdef USING_WINDOWS_THREADS
	DWORD dwRes;

	if (WaitForSingleObject(t, INFINITE) == WAIT_FAILED)
		return thrd_error;

	if (res != NULL)
	{
		if (GetExitCodeThread(t, &dwRes) != 0)
			*res = (int) dwRes;
		else
			return thrd_error;
	}

	CloseHandle(t);
#else
	void *pres;
	if (pthread_join(t, &pres) != 0)
		return thrd_error;

	if (res != NULL)
		*res = (int)(intptr_t)pres;
#endif

	return thrd_success;
}

int
thrd_sleep(const struct timespec *duration, struct timespec *remaining)
{
#ifdef USING_POSIX_THREADS
	int res = nanosleep(duration, remaining);
	if (res == 0)
		return 0;
	else if (errno == EINTR)
		return -1;
	else
		return -2;
#else
	struct timespec start;

	timespec_get(&start, TIME_UTC);

	unsigned long long milliseconds = duration_milliseconds(*duration);

	if (milliseconds != ((unsigned long long)(DWORD)milliseconds))
		return -2;

	DWORD t = SleepEx((DWORD)milliseconds, TRUE);

	if (t == 0)
		return 0;

	if (remaining != NULL) {
		timespec_get(remaining, TIME_UTC);
		remaining->tv_sec -= start.tv_sec;
		remaining->tv_nsec -= start.tv_nsec;
		if (remaining->tv_nsec < 0) {
			remaining->tv_nsec += 1000000000;
			remaining->tv_sec -= 1;
		}
	}

	if (t == WAIT_IO_COMPLETION)
		return -1;
	else
		return -2;
#endif
}

void
thrd_yield(void)
{
#ifdef USING_POSIX_THREADS
	sched_yield();
#else
	SwitchToThread();
#endif
}

int
mtx_init(mtx_t *opaque_mtx, int type)
{
	system_mtx_t *mtx = (system_mtx_t*) opaque_mtx;
#ifdef USING_WINDOWS_THREADS
	mtx->is_already_locked = false;
	mtx->is_recursive = (type & mtx_recursive) == mtx_recursive;
	mtx->is_timed = (type & mtx_timed) == mtx_timed;

	if (!mtx->is_timed) {
		InitializeCriticalSection(&(mtx->handle.cs));
	}
	else {
		mtx->handle.mut = CreateMutex(NULL, false, NULL);
		if (mtx->handle.mut == NULL)
			return thrd_error;
	}

	return thrd_success;
#else
	int ret;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	if (type & mtx_recursive)
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	ret = pthread_mutex_init(mtx, &attr);
	pthread_mutexattr_destroy(&attr);
	return ret == 0 ? thrd_success : thrd_error;
#endif
}

void
mtx_destroy(mtx_t *opaque_mtx)
{
	system_mtx_t *mtx = (system_mtx_t*) opaque_mtx;
#ifdef USING_WINDOWS_THREADS
	if (!mtx->is_timed)
		DeleteCriticalSection(&(mtx->handle.cs));
	else
		CloseHandle(mtx->handle.mut);
#else
	pthread_mutex_destroy(mtx);
#endif
}

int
mtx_lock(mtx_t *opaque_mtx)
{
	system_mtx_t *mtx = (system_mtx_t*) opaque_mtx;

#ifdef USING_WINDOWS_THREADS
	if (!mtx->is_timed) {
		EnterCriticalSection(&(mtx->handle.cs));
		if (mtx->is_already_locked && !mtx->is_recursive) {
			LeaveCriticalSection(&(mtx->handle.cs));
			return thrd_error;
		}
	}
	else {
		switch (WaitForSingleObject(mtx->handle.mut, INFINITE)) {
			case WAIT_OBJECT_0:
				if (mtx->is_already_locked && !mtx->is_recursive)
					return thrd_error;
				break;
			case WAIT_ABANDONED:
			default:
				return thrd_error;
		}
	}

	if (!mtx->is_recursive)
		mtx->is_already_locked = true;

	return thrd_success;
#else
	return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
#endif
}

static bool
is_ts_in_past(const struct timespec current, const struct timespec ts)
{
	if (current.tv_sec > ts.tv_sec)
		return true;

	if (current.tv_sec < ts.tv_sec)
		return false;

	return current.tv_nsec > ts.tv_nsec;
}

int
mtx_timedlock(mtx_t *restrict opaque_mtx, const struct timespec *restrict ts)
{
	system_mtx_t *mtx = (system_mtx_t*) opaque_mtx;

#ifdef USING_WINDOWS_THREADS
	struct timespec current_ts;
	unsigned long long milliseconds;

	if (!mtx->is_timed)
		return thrd_error;

	timespec_get(&current_ts, TIME_UTC);

	if (is_ts_in_past(current_ts, ts)) {
		milliseconds = 0;
	}
	else {
		milliseconds  = (DWORD)(ts->tv_sec  - current_ts.tv_sec)  * 1000;
		milliseconds += (ts->tv_nsec - current_ts.tv_nsec) / 1000000;
		milliseconds += 1;
	}

	if (milliseconds != ((unsigned long long)(DWORD)milliseconds))
		return thrd_error;

	/*
	 * TODO: the timeout for WaitForSingleObject doesn't include time
	 * while the computer is asleep.
	 */
	switch (WaitForSingleObject(mtx->handle.mut, (DWORD)milliseconds))
	{
		case WAIT_OBJECT_0:
			break;
		case WAIT_TIMEOUT:
			return thrd_timedout;
		case WAIT_ABANDONED:
		default:
			return thrd_error;
	}

	if (!mtx->is_recursive)
	{
		while(mtx->is_already_locked)
			Sleep(1); // Simulate deadlock...
		mtx->is_already_locked = true;
	}

	return thrd_success;
#elif defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L) \
	&& defined(_POSIX_THREADS) && (_POSIX_THREADS >= 200112L)

	(void) is_ts_in_past;

	switch (pthread_mutex_timedlock(mtx, ts)) {
		case 0:
			return thrd_success;
		case ETIMEDOUT:
			return thrd_timedout;
		default:
			return thrd_error;
	}
#else
	int rc;
	struct timespec cur, dur;

	/* Try to acquire the lock and, if we fail, sleep for 5ms. */
	while ((rc = pthread_mutex_trylock(mtx)) == EBUSY) {
		timespec_get(&cur, TIME_UTC);

		if (is_ts_in_past(cur, ts))
			break;

		dur.tv_sec = ts->tv_sec - cur.tv_sec;
		dur.tv_nsec = ts->tv_nsec - cur.tv_nsec;
		if (dur.tv_nsec < 0) {
			dur.tv_sec--;
			dur.tv_nsec += 1000000000;
		}

		if ((dur.tv_sec != 0) || (dur.tv_nsec > 5000000)) {
			dur.tv_sec = 0;
			dur.tv_nsec = 5000000;
		}

		nanosleep(&dur, NULL);
	}

	switch (rc) {
		case 0:
			return thrd_success;
		case ETIMEDOUT:
		case EBUSY:
			return thrd_timedout;
		default:
			return thrd_error;
	}
#endif
}

int
mtx_trylock(mtx_t *mtx)
{
	system_mtx_t *mtx = (system_mtx_t*) opaque_mtx;

#ifdef USING_WINDOWS_THREADS
	int ret = thrd_success;

	if (!mtx->is_timed) {
		if (!TryEnterCriticalSection(&(mtx->handle.cs)))
			ret = thrd_busy;
	}
	else {
		if (WaitForSingleObject(mtx->handle.mut, 0) != WAIT_OBJECT_0)
			ret = thrd_busy;
	}

	if ((!mtx->is_recursive) && (ret == thrd_success)) {
		if (mtx->is_already_locked) {
			LeaveCriticalSection(&(mtx->handle.cs));
			ret = thrd_busy;
		}
		else {
			mtx->is_already_locked = true;
		}
	}
	return ret;
#else
	if (pthread_mutex_trylock(mtx) == 0)
		return thrd_success;
	else
		return thrd_busy;
#endif
}

int mtx_unlock(mtx_t *opaque_mtx)
{
	system_mtx_t *mtx = (system_mtx_t*) opaque_mtx;

#ifdef USING_WINDOWS_THREADS
	mtx->is_already_locked = false;
	if (!mtx->is_timed) {
		LeaveCriticalSection(&(mtx->handle.cs));
	}
	else {
		if (!ReleaseMutex(mtx->handle.mut))
			return thrd_error;
	}

	return thrd_success;
#else
	if (pthread_mutex_unlock(mtx) == 0)
		return thrd_success;
	else
		return thrd_error;
#endif
}

int
cnd_broadcast(cnd_t *opaque_cond)
{
	system_cnd_t *cond = (system_cnd_t*) opaque_cond;

#ifdef USING_WINDOWS_THREADS
	bool waiters_present;

	/* Are there any waiters? */
	EnterCriticalSection(&cond->waiters_count_lock);
	waiters_present = (cond->waiters_count > 0);
	LeaveCriticalSection(&cond->waiters_count_lock);

	/* If we have any waiting threads, send them a signal */
	if (waiters_present) {
		if (SetEvent(cond->events[condition_event_all]) == 0)
			return thrd_error;
	}

	return thrd_success;
#else
	if (pthread_cond_broadcast(cond) == 0)
		return thrd_success;
	else
		return thrd_error;
#endif
}

void
cnd_destroy(cnd_t *opaque_cond)
{
	system_cnd_t *cond = (system_cnd_t*) opaque_cond;

#ifdef USING_WINDOWS_THREADS
	if (cond->events[condition_event_one] != NULL)
		CloseHandle(cond->events[condition_event_one]);
	if (cond->events[condition_event_all] != NULL)
		CloseHandle(cond->events[condition_event_all]);
	DeleteCriticalSection(&cond->waiters_count_lock);
#else
	pthread_cond_destroy(cond);
#endif
}

int
cnd_init(cnd_t *opaque_cond)
{
	system_cnd_t *cond = (system_cnd_t*) opaque_cond;

#ifdef USING_WINDOWS_THREADS
	cond->waiters_count = 0;

	/* Init critical section */
	InitializeCriticalSection(&cond->waiters_count_lock);

	/* Init events */
	cond->events[condition_event_one] = CreateEvent(NULL, false, false, NULL);
	if (cond->events[condition_event_one] == NULL) {
		cond->events[condition_event_all] = NULL;
		return thrd_error;
	}

	cond->events[condition_event_all] = CreateEvent(NULL, true, false, NULL);

	if (cond->events[condition_event_all] == NULL) {
		CloseHandle(cond->events[condition_event_one]);
		cond->events[condition_event_one] = NULL;
		return thrd_error;
	}

	return thrd_success;
#else
	if (pthread_cond_init(cond, NULL) == 0)
		return thrd_success;
	else
		return thrd_error;
#endif
}

int
cnd_signal(cnd_t *opaque_cond)
{
	system_cnd_t *cond = (system_cnd_t*) opaque_cond;

#ifdef USING_WINDOWS_THREADS
	bool waiters_present;

	/* Are there any waiters? */
	EnterCriticalSection(&cond->waiters_count_lock);
	waiters_present = (cond->waiters_count > 0);
	LeaveCriticalSection(&cond->waiters_count_lock);

	/* If we have any waiting threads, send them a signal */
	if (waiters_present) {
		if (SetEvent(cond->events[condition_event_one]) == 0)
			return thrd_error;
	}

	return thrd_success;
#else
	if (pthread_cond_signal(cond) == 0)
		return thrd_success;
	else
		return thrd_error;
#endif
}

int
cnd_timedwait(cnd_t *restrict opaque_cond, mtx_t *restrict opaque_mtx,
	      const struct timespec *restrict ts)
{
	system_mtx_t *mtx = (system_mtx_t*) opaque_mtx;
	system_cnd_t *cond = (system_cnd_t*) opaque_cond;

#ifdef USING_WINDOWS_THREADS
	struct timespec now;
	if (timespec_get(&now, TIME_UTC) == TIME_UTC)
	{
		unsigned long long nowInMilliseconds =
		    now.tv_sec * 1000 + now.tv_nsec / 1000000;
		unsigned long long tsInMilliseconds  =
		    ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
		DWORD delta = (tsInMilliseconds > nowInMilliseconds)
		    ?
		    (DWORD)(tsInMilliseconds
			    - nowInMilliseconds)
		    : 0;
		return
		    _cnd_timedwait_win32(cond,
					 mtx,
					 delta);
	}
	else
		return thrd_error;
#else
	int ret;
	ret = pthread_cond_timedwait(cond, mtx, ts);
	if (ret == ETIMEDOUT)
	{
		return thrd_timedout;
	}
	return ret == 0 ? thrd_success : thrd_error;
#endif
}



#endif
