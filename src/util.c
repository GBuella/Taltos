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

#ifndef TALTOS_CAN_USE_ISO_ALIGNAD_ALLOC
#ifdef TALTOS_CAN_USE_INTEL_MMALLOC
#include <xmmintrin.h>
#endif
#endif

#include "util.h"

#include <stdlib.h>
#include <stdio.h>
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
