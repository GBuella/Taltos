/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2020, Gabor Buella
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

#include "taltos_config.h"

#ifdef TALTOS_CAN_USE_CLOCK_GETTIME
#define _POSIX_C_SOURCE 199309L
#endif

#include "util.h"

#ifdef TALTOS_CAN_USE_CLOCK_GETTIME
#include <time.h>
#elif defined(TALTOS_CAN_USE_W_PERFCOUNTER)
#include <Windows.h>
#include <io.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

taltos_systime
xnow(void)
{
	taltos_systime result;

#ifdef TALTOS_CAN_USE_CLOCK_GETTIME

	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
		fputs("Error calling clock_gettime\n", stderr);
		abort();
	}
	static_assert(sizeof(now) <= sizeof(result), "internal error");
	memcpy(result.data, &now, sizeof(now));

#elif defined(TALTOS_CAN_USE_W_PERFCOUNTER)

	static_assert(sizeof(LARGE_INTEGER) == sizeof(uint64_t),
	    "unexpectedly large integer");

	LARGE_INTEGER perf_count;

	if (!QueryPerformanceCounter(&perf_count)) {
		fputs("Error calling QueryPerformanceCounter\n", stderr);
		abort();
	}

	static_assert(sizeof(perf_count.QuadPart) <= sizeof(result),
		      "internal error");
	memcpy(result.data, &perf_count.QuadPart);
#else
#error unable to use monotonic clock
#endif

	return result;
}

uintmax_t
xseconds_since(taltos_systime prev_time_raw)
{
#ifdef TALTOS_CAN_USE_CLOCK_GETTIME

	struct timespec prev_time;
	memcpy(&prev_time, prev_time_raw.data, sizeof(prev_time));

	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	now.tv_sec -= prev_time.tv_sec;
	if (prev_time.tv_nsec > now.tv_nsec) {
		now.tv_sec--;
		now.tv_nsec += 1000000000;
	}
	now.tv_nsec -= prev_time.tv_nsec;
	return now.tv_sec * 100 + now.tv_nsec / 10000000;

#elif defined(TALTOS_CAN_USE_W_PERFCOUNTER)

	static LARGE_INTEGER pcounter_frequency;

	uintmax_t prev_time;
	uintmax_t now;
	memcpy(&prev_time, prev_time_raw.data, sizeof(prev_time));

	taltos_systime now_raw = xnow();
	memcpy(&now, now_raw.data, sizeof(now));

	if (pcounter_frequency == 0) {
		if (!QueryPerformanceFrequency(&pcounter_frequency))
			abort();
	}

	return (now - prev_time) / (pcounter_frequency.QuadPart / 100);

#else

#error unable to use monotonic clock

#endif
}
