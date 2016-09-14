
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_PERFT_H
#define TALTOS_PERFT_H

#include "chess.h"

struct divide_info;

unsigned long perft(const struct position*, unsigned depth)
	attribute(nonnull);
unsigned long qperft(const struct position*, unsigned depth)
	attribute(nonnull);
unsigned long perft_ordered(const struct position*, unsigned depth)
	attribute(nonnull);
unsigned long perft_distinct(const struct position*, unsigned depth)
	attribute(nonnull);
struct divide_info*
divide_init(const struct position*, unsigned depth, enum player, bool ordered)
	attribute(nonnull, returns_nonnull);
const char *divide(struct divide_info*, enum move_notation_type)
	attribute(nonnull);
void divide_destruct(struct divide_info*);

#endif
