/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
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
#include "util.h"

#include <cstring>

namespace taltos
{


void* alloc_align64(std::size_t size)
{
	uintptr_t actual = (uintptr_t)(new char[size + 128]);

	uintptr_t address = (actual + 128) - (actual % 64);

	uintptr_t* x = (uintptr_t*) address;
	x[-1] = actual;

	return (void*) address;
}

void free_align64(void* address)
{
	if (address != nullptr) {
		uintptr_t* x = (uintptr_t*) address;
		char* actual = (char*) x[-1];
		delete[] actual;
	}
}

// http://pubs.opengroup.org/onlinepubs/9699919799/functions/strtok.html
char*
xstrtok_r(char *restrict str, const char *restrict sep, char **restrict lasts)
{
	if (str == nullptr) {
		str = *lasts;
		if (str == nullptr)
			return nullptr;
	}

	str += strspn(str, sep);
	if (*str != '\0') {
		char *end = str + strcspn(str, sep);

		if (*end == '\0') {
			*lasts = nullptr;
		}
		else {
			*end = '\0';
			*lasts = end + 1;
		}
		return str;
	}
	else {
		return nullptr;
	}
}

}
