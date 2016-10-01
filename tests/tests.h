/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_TESTS_H
#define TALTOS_TESTS_H

/*
 * Common header for tests.
 * Should be included first.
 * Getting rid of the NDEBUG macro, so asserts in tests
 * work in Release builds as well.
 */

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>

#include "macros.h"

void run_tests(void);

#endif
