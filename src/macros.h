
#ifndef MACROS_H
#define MACROS_H

#include "taltos_config.h"

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
#define QUOTE(x) #x
#define STR(x) QUOTE(x)

#if defined(unix) || defined(__unix) || defined(__unix__) || defined(UNIX) \
    || (defined(__APPLE__) && defined(__MACH__))
#   define POSIX_BUILD 1
#endif

#ifdef TALTOS_CAN_USE_BUILTIN_UNREACHABLE
#define unreachable __builtin_unreachable()
#elif defined(_MSC_VER)
#define unreachable __assume(0)
#else
#define unreachable { assert(0); }
#endif

#ifdef TALTOS_CAN_USE_ATTRIBUTE_MALLOC
#define attribute_malloc __attribute__((malloc))
#else
#define attribute_malloc
#endif

#ifdef TALTOS_CAN_USE_ATTRIBUTE_ALLOC_SIZE
#define attribute_alloc_size(size) __attribute__((alloc_size(size)))
#else
#define attribute_alloc_size(size)
#endif

#ifdef TALTOS_CAN_USE_ATTRIBUTE_LEAF
#define attribute_leaf __attribute__((leaf))
#else
#define attribute_leaf
#endif

#ifdef TALTOS_CAN_USE_ATTRIBUTE_CONST
#define attribute_const __attribute__((const))
#else
#define attribute_const
#endif

#ifdef TALTOS_CAN_USE_ATTRIBUTE_PURE
#define attribute_pure __attribute__((pure))
#else
#define attribute_pure
#endif

#ifdef TALTOS_CAN_USE_ATTRIBUTE_PRINTF
#define attribute_printf(...) __attribute__((format(printf, __VA_ARGS__)))
#else
#define attribute_printf(...)
#endif

#ifdef TALTOS_CAN_USE_ATTRIBUTE_NONNULL
#define attribute_args_nonnull(...) __attribute__((nonnull(__VA_ARGS__)))
#define attribute_nonnull __attribute__((nonnull))
#else
#define attribute_args_nonnull(...)
#define attribute_nonnull
#endif

#ifdef TALTOS_CAN_USE_ATTRIBUTE_RET_NONNULL
#define attribute_returns_nonnull __attribute__((returns_nonnull))
#else
#define attribute_returns_nonnull
#endif

#ifdef TALTOS_CAN_USE_ATTRIBUTE_WARN_UNUSED_RESULT
#define attribute_warn_unused_result __attribute__((warn_unused_result))
#else
#define attribute_warn_unused_result
#endif

#ifdef TALTOS_CAN_USE_BUILTIN_PREFETCH
#define prefetch __builtin_prefetch
#else
#define prefetch()
#endif

#ifdef __GNUC__

#   if __LP64__
#   define BUILD_64 1
#   endif

#elif defined(_MSC_VER)

#   if defined(WIN64) || defined(_WIN64) || defined(__WIN64__)
#       define BUILD_64 1
#   endif

#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ > 201100L) \
        && !defined(__STDC_NO_ATOMICS__)
#   define HAS_C11_ATOMICS
#   if !defined(__STDC_NO_THREADS__)
#       define HAS_C11_THREADS
#   endif
#endif

#define MILLION (1000 * 1000)
#define BILLION (1000 * MILLION)

/* From Unicode Mathematical Alphanumeric Symbols */
#define U_ALPHA "\U0001d6fc"
#define U_BETA "\U0001d6fd"
#define U_MU "\U0001d707"

#endif /* MACROS_H */
