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

char *xstrtok_r(char *restrict str, const char *restrict sep,
		char **restrict lasts)
	attribute(nonnull(2, 3));

void util_init(void);

#endif
