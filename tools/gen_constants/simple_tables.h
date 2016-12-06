
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_UTIL_SIMPLE_TABLES_H
#define TALTOS_UTIL_SIMPLE_TABLES_H

#include <stdint.h>

void gen_knight_table(uint64_t table[static 64]);
void gen_king_table(uint64_t table[static 64]);

#endif
