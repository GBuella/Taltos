
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_UTIL_H
#define TALTOS_UTIL_H

#include <stdio.h>
#include <stdint.h>
#include "macros.h"

#ifdef TALTOS_CAN_USE_CLOCK_GETTIME
#include <time.h>
#endif

void *xmalloc(size_t)
	attribute(warn_unused_result, malloc, alloc_size(1));

void *xcalloc(size_t count, size_t size)
	attribute(warn_unused_result, malloc, alloc_size(1, 2));

void *xrealloc(void*, size_t)
	attribute(warn_unused_result, alloc_size(2));

/*
 * aligned_alloc might fail ( return NULL )
 * xaligned_alloc does abort(3) on failure
 * pointers from both must be free'd using xaligned_free
 */
#ifndef TALTOS_CAN_USE_ISO_ALIGNAD_ALLOC
// damn you, libc on my laptop in 2016
// y no no have aligned_alloc from ISOC ??
void *aligned_alloc(size_t alignment, size_t size)
	attribute(warn_unused_result, malloc, alloc_size(2), alloc_align(1));
#endif

void *xaligned_alloc(size_t alignment, size_t size)
	attribute(warn_unused_result, malloc, alloc_size(2), alloc_align(1));

void *xaligned_calloc(size_t alignment, size_t count, size_t size)
	attribute(warn_unused_result, malloc, alloc_size(2, 3), alloc_align(1));

void xaligned_free(void*);

int bin_file_size(FILE*, size_t*)
	attribute(warn_unused_result, nonnull(2));

#if defined(TALTOS_CAN_USE_CLOCK_GETTIME)
typedef struct timespec taltos_systime;
#elif defined(TALTOS_CAN_USE_MACH_ABS_TIME) || \
	defined(TALTOS_CAN_USE_W_PERFCOUNTER)
typedef uint64_t taltos_systime;
#else
#error unable to use monotonic clock
#endif

taltos_systime xnow(void);
uintmax_t xseconds_since(taltos_systime);
uintmax_t get_big_endian_num(size_t size, const unsigned char[size]);

#endif
