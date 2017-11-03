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

#ifndef TALTOS_MACROS_H
#define TALTOS_MACROS_H

#include "taltos_config.h"

#include <cassert>

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
#define QUOTE(x) #x
#define STR(x) QUOTE(x)

#if defined(NDEBUG) && defined(__GNUC__)

#define unreachable __builtin_unreachable()
#define invariant(x) { if (!(x)) unreachable; }

#elif defined(NDEBUG) && defined(_MSC_VER)

#define unreachable __assume(0)
#define invariant __assume

#else

#include <cstdlib>

#define unreachable abort()
#define invariant assert

#endif

#ifdef __GNUC__
#define prefetch __builtin_prefetch
#else
#define prefetch(...)
#endif

#ifndef TALTOS_CAN_USE_RESTRICT_KEYWORD
#ifdef TALTOS_CAN_USE___RESTRICT_KEYWORD
#define restrict __restrict
#else
#define restrict
#endif
#endif

/* From Unicode Mathematical Alphanumeric Symbols */
#define U_ALPHA "\U0001d6fc"
#define U_BETA "\U0001d6fd"
#define U_MU "\U0001d707"

#endif /* TALTOS_MACROS_H */
