/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2017, Gabor Buella
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ISO_C11_THREADS_IMPLEMENTATION_H
#define ISO_C11_THREADS_IMPLEMENTATION_H

/*
 * 7.26.1.1
 * The header <threads.h> includes the header <time.h>, defines macros,
 * and declares types, enumeration constants, and functions that support
 * multiple threads of execution.
 */

#include <time.h>

/* 7.26.1.3 macros */

#define thread_local _Thread_local
#define ONCE_FLAG_INIT {{0, 0, 0, 0, 0, 0, 0, 0}, 0}
#define TSS_DTOR_ITERATIONS 4

/* 7.26.1.4 types */

typedef struct {
	long _opaque_data[8];
} cnd_t;

typedef struct {
	long long _opaque_data[2];
} thrd_t;

typedef long long tss_t;

typedef struct {
	long _opaque_data[8];
} mtx_t;

typedef void (*tss_dtor_t)(void *val);

typedef int (*thrd_start_t)(void *arg);

typedef struct {
	mtx_t _lock;
	int _status;
} once_flag;

/* 7.26.1.5 enumeration constants */

enum {
	mtx_plain,
	mtx_timed,
	mtx_recursive
};

enum {
	/*
	 * The time specified in the call was reached without acquiring the
	 * requested resource
	 */
	thrd_timedout,

	/* The requested operation succeeded */
	thrd_success,

	/*
	 * The requested operation failed because a tesource requested by a test
	 * and return function is already in use
	 */
	thrd_busy,

	/* The requested operation failed */
	thrd_error,

	/*
	 * returned by a function to indicate that the requested operation
	 * failed because it was unable to allocate memory
	 */
	thrd_nomem
};

/* 7.26.2 Initialization functions */

void call_once(once_flag *flag, void (*func)(void));

/* 7.26.3 Condition variable functions */

int cnd_broadcast(cnd_t *cond);
void cnd_destroy(cnd_t *cond);
int cnd_init(cnd_t *cond);
int cnd_signal(cnd_t *cond);
int cnd_timedwait(cnd_t *restrict cond, mtx_t *restrict mtx,
		  const struct timespec *restrict ts);

/* 7.26.4 Mutex functions */

void mtx_destroy(mtx_t *mtx);
int mtx_init(mtx_t *mtx, int type);
int mtx_lock(mtx_t *mtx);
int mtx_timedlock(mtx_t *restrict mtx, const struct timespec *restrict ts);
int mtx_trylock(mtx_t *mtx);
int mtx_unlock(mtx_t *mtx);

/* 7.26.5 Thread functions */

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
thrd_t thrd_current(void);
int thrd_detach(thrd_t thr);
int thrd_equal(thrd_t thr0, thrd_t thr1);
_Noreturn void thrd_exit(int res);
int thrd_join(thrd_t thr, int *res);
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);
void thrd_yield(void);

/* 7.26.6 Thread-specific storage functions */

int tss_create(tss_t *key, tss_dtor_t dtor);
void tss_delete(tss_t key);
void *tss_get(tss_t key);
int tss_set(tss_t key, void *val);

#endif
