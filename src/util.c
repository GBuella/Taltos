
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "taltos_config.h"

#if defined(TALTOS_CAN_USE_CLOCK_GETTIME) || \
	defined(TALTOS_CAN_USE_POSIX_FSTAT)
#define _POSIX_C_SOURCE 199309L
#endif

#ifdef TALTOS_CAN_USE_POSIX_FSTAT
#include <stdio.h>
#include <sys/stat.h>
#elif TALTOS_CAN_USE_W_GETFILESIZEEX
#include <Windows.h>
#else
#error dont know how to get filesize
#endif

#ifdef TALTOS_CAN_USE_GETRUSAGE
#include <sys/resource.h>
#else
#error todo
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
#else
#error todo
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

/*CSTYLED*/
// https://www.securecoding.cert.org/confluence/display/c/FIO19-C.+Do+not+use+fseek%28%29+and+ftell%28%29+to+compute+the+size+of+a+regular+file
/*
 * "Setting the file position indicator to end-of-file, as
 * with fseek(file, 0, SEEK_END), has undefined behavior for a binary stream"
 *                                          ISO-9899-2011 7.21.3
 */
int
bin_file_size(FILE *file, size_t *size)
{
#ifdef TALTOS_CAN_USE_POSIX_FSTAT
	struct stat stat;
	if (fstat(fileno(file), &stat) != 0) {
		return -1;
	}
	*size = stat.st_size;
	return 0;
#elif TALTOS_CAN_USE_W_GETFILESIZEEX
#error todo
#endif
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

#else
#error todo
#endif
}

#if defined(TALTOS_CAN_USE_MACH_ABS_TIME) \
	&& defined(TALTOS_CAN_USE_CONSTRUCTOR_ATTRIBUTE)

static mach_timebase_info_data_t timebase_info;

static attribute(constructor) void
init_timebase(void)
{
	if (mach_timebase_info(&timebase_info) != KERN_SUCCESS)
		abort();
}

#endif

uintmax_t
xseconds_since(taltos_systime some_time_ago)
{
#ifdef TALTOS_CAN_USE_MACH_ABS_TIME

	uint64_t now = mach_absolute_time();

#ifndef TALTOS_CAN_USE_CONSTRUCTOR_ATTRIBUTE
	mach_timebase_info_data_t timebase_info;
	if (mach_timebase_info(&timebase_info) != KERN_SUCCESS) {
		puts("Error calling mach_timebase_info", stderr);
		abort();
	}
#endif

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
#else
#error todo
#endif
}

uintmax_t
get_big_endian_num(size_t size, const unsigned char str[size])
{
	uintmax_t value = 0;

	while (size > 0) {
		value = (value << 8) + *str;
		++str;
		--size;
	}
	return value;
}