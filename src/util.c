/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2017, Gabor Buella
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

#if defined(TALTOS_CAN_USE_CLOCK_GETTIME)
#define _POSIX_C_SOURCE 199309L
#endif

#if defined(TALTOS_CAN_USE_W_PERFCOUNTER)
#include <Windows.h>
#ifdef TALTOS_CAN_USE_W_PERFCOUNTER
#include <io.h>
#endif
#endif

#ifdef TALTOS_CAN_USE_GETRUSAGE
#include <sys/resource.h>
#endif

#ifndef TALTOS_CAN_USE_ISO_ALIGNAD_ALLOC
#ifdef TALTOS_CAN_USE_INTEL_MMALLOC
#include <xmmintrin.h>
#endif
#endif

#ifdef TALTOS_CAN_USE_MACH_ABS_TIME
#include <CoreServices/CoreServices.h>
#include <mach/mach_time.h>
#elif defined(TALTOS_CAN_USE_CLOCK_GETTIME)
#include <time.h>
#endif

#include "util.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static void*
check_allocation(void *address, size_t size)
{
	if (address == NULL && size > 0) {
		fprintf(stderr, "Error allocating %zu bytes\n", size);
		abort();
	}
	return address;
}

void*
xmalloc(size_t size)
{
	return check_allocation(malloc(size), size);
}

void*
xcalloc(size_t count, size_t size)
{
	return check_allocation(calloc(count, size), count * size);
}

void*
xrealloc(void *address, size_t size)
{
	return check_allocation(realloc(address, size), size);
}

#ifndef TALTOS_CAN_USE_ISO_ALIGNAD_ALLOC
#ifdef TALTOS_CAN_USE_INTEL_MMALLOC
void*
aligned_alloc(size_t alignment, size_t size)
{
	return _mm_malloc(size, alignment);
}

void
xaligned_free(void *address)
{
	_mm_free(address);
}
#else
#error time to roll your own aligned_alloc should be easy
#endif
#else
void
xaligned_free(void *address)
{
	free(address);
}
#endif

void*
xaligned_alloc(size_t alignment, size_t size)
{
	void *address = aligned_alloc(alignment, size);
	if (((uintptr_t)address) % alignment != 0) {
		fprintf(stderr,
		    "Error allocating %zu bytes: alignment mismatch\n", size);
		abort();
	}
	return check_allocation(address, size);
}

void*
xaligned_calloc(size_t alignment, size_t count, size_t size)
{
	size *= count;
	void *address = xaligned_alloc(alignment, size);
	memset(address, 0, size);
	return address;
}

taltos_systime
xnow(void)
{
#ifdef TALTOS_CAN_USE_MACH_ABS_TIME

	return mach_absolute_time();

#elif defined(TALTOS_CAN_USE_CLOCK_GETTIME)

	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
		fputs("Error calling clock_gettime\n", stderr);
		abort();
	}
	return now;

#elif defined(TALTOS_CAN_USE_W_PERFCOUNTER)

	static_assert(sizeof(LARGE_INTEGER) == sizeof(uint64_t),
	    "unexpectedly large integer");

	LARGE_INTEGER result;

	if (!QueryPerformanceCounter(&result)) {
		fputs("Error calling QueryPerformanceCounter\n", stderr);
		abort();
	}

	return result.QuadPart;
#else
#error unable to use monotonic clock
#endif
}

#ifdef TALTOS_CAN_USE_MACH_ABS_TIME
static mach_timebase_info_data_t timebase_info;
#elif defined(TALTOS_CAN_USE_W_PERFCOUNTER)
static LARGE_INTEGER pcounter_frequency;
#endif

uintmax_t
xseconds_since(taltos_systime some_time_ago)
{
#ifdef TALTOS_CAN_USE_MACH_ABS_TIME

	uint64_t now = mach_absolute_time();

	return (now - some_time_ago) * timebase_info.numer / timebase_info.denom
	    / 10000000;

#elif defined(TALTOS_CAN_USE_CLOCK_GETTIME)

	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	now.tv_sec -= some_time_ago.tv_sec;
	if (some_time_ago.tv_nsec > now.tv_nsec) {
		now.tv_sec--;
		now.tv_nsec += 1000000000;
	}
	now.tv_nsec -= some_time_ago.tv_nsec;
	return now.tv_sec * 100 + now.tv_nsec / 10000000;

#elif defined(TALTOS_CAN_USE_W_PERFCOUNTER)

	return (xnow() - some_time_ago) / (pcounter_frequency.QuadPart / 100);

#else
#error unable to use monotonic clock
#endif
}

/*CSTYLED*/
// http://pubs.opengroup.org/onlinepubs/9699919799/functions/strtok.html
char*
xstrtok_r(char *restrict str, const char *restrict sep, char **restrict lasts)
{
	if (str == NULL) {
		str = *lasts;
		if (str == NULL)
			return NULL;
	}

	str += strspn(str, sep);
	if (*str != '\0') {
		char *end = str + strcspn(str, sep);

		if (*end == '\0') {
			*lasts = NULL;
		}
		else {
			*end = '\0';
			*lasts = end + 1;
		}
		return str;
	}
	else {
		return NULL;
	}
}

void
util_init(void)
{
#if defined(TALTOS_CAN_USE_MACH_ABS_TIME)
	if (mach_timebase_info(&timebase_info) != KERN_SUCCESS)
		abort();
#elif defined(TALTOS_CAN_USE_W_PERFCOUNTER)
	if (!QueryPerformanceFrequency(&pcounter_frequency))
		abort();
#endif
}
