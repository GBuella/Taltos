
#ifndef MACROS_H
#define MACROS_H

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
#define QUOTE(x) #x
#define STR(x) QUOTE(x)

#if defined(unix) || defined(__unix) || defined(__unix__) || defined(UNIX) \
    || (defined(__APPLE__) && defined(__MACH__))
#   define POSIX_BUILD 1
#endif

#ifdef __GNUC__

#   define UNUSED __attribute__ (( unused ))
#   define PACKED __attribute__ (( packed ))
#   define PREFETCH(x) __builtin_prefetch(x)
#   ifdef NDEBUG
#       if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 4)
#           define unreachable() __builtin_unreachable()
#       else
#           define unreachable() { return; }
#       endif
#   else
#       define unreachable() assert(0)
#   endif

#   if __LP64__
#   define BUILD_64 1
#   if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 6)
#       define NO_VECTOR_SUBSCRIPT 1
#   endif
#   else /*  __LP64__ */
#   define NO_VECTOR_SUBSCRIPT 1
#   endif /*  !__LP64__ */

#elif defined(_MSC_VER)

#   define UNUSED
#   define PACKED
#   ifdef NDEBUG
#       define unreachable() __assume(0)
#   else
#       define unreachable() assert(0)
#   endif
#   define PREFETCH(x)
#   define NO_VECTOR_SUBSCRIPT 1

#   if defined(WIN64) || defined(_WIN64) || defined(__WIN64__)
#       define BUILD_64 1
#   else /* WIN64 */
#   endif /* !WIN64 */

#else /* GNUC || MSC_VER */

#   define UNUSED
#   define PACKED
#   ifdef NDEBUG
#       define unreachable() { return; }
#   else
#       define unreachable() assert(0)
#   endif
#   define PREFETCH(x)
#   define NO_VECTOR_SUBSCRIPT 1

#endif /* GNUC || MSC_VER */

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

