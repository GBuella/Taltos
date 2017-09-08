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

extern int prog_argc;
extern const char **prog_argv;

struct game;

const struct game *parse_setboard_from_arg_file(void);

void run_tests(void);

#endif
