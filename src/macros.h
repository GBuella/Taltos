
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_MACROS_H
#define TALTOS_MACROS_H

#include "taltos_config.h"

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
#define QUOTE(x) #x
#define STR(x) QUOTE(x)

#if defined(NDEBUG) && defined(TALTOS_CAN_USE_BUILTIN_UNREACHABLE)

#define unreachable __builtin_unreachable()
#define invariant(x) { if (!(x)) unreachable; }

#elif defined(NDEBUG) && defined(_MSC_VER)

#define unreachable __assume(0)
#define invariant __assume

#else

#include <assert.h>

#define unreachable assert(0)
#define invariant assert

#endif

#ifdef TALTOS_CAN_USE_GNU_ATTRIBUTE_SYNTAX
#define attribute(...) __attribute__((__VA_ARGS__))
#else
#define attribute(...)
#endif

#ifdef TALTOS_CAN_USE_BUILTIN_PREFETCH
#define prefetch __builtin_prefetch
#else
#define prefetch(...)
#endif

#define MILLION (1000 * 1000)
#define BILLION (1000 * MILLION)

/* From Unicode Mathematical Alphanumeric Symbols */
#define U_ALPHA "\U0001d6fc"
#define U_BETA "\U0001d6fd"
#define U_MU "\U0001d707"

#endif /* TALTOS_MACROS_H */
