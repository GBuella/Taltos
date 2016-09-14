
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_TESTS_H
#define TALTOS_TESTS_H

#ifdef TALTOS_BUILD_WITHOUT_TESTS

static inline void run_internal_tests(void) { }

#else

void run_internal_tests(void);
void run_hash_table_tests(void);
void run_string_tests(void);

#endif
#endif
