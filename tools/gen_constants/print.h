
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_UTIL_PRINT_H
#define TALTOS_UTIL_PRINT_H

#include "macros.h"

#include <stddef.h>
#include <stdint.h>

void print_table(size_t size, const uint64_t table[static size],
			const char *name);

void print_table_byte(size_t size, const uint8_t table[static size],
			const char *name);

void print_table_2d(size_t s0, size_t s1, const uint64_t table[static s0 * s1],
			const char *name);

#endif
