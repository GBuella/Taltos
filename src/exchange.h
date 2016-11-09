
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_STATIC_EXCHANGE_H
#define TALTOS_STATIC_EXCHANGE_H

#include "macros.h"
#include "chess.h"

int SEE_move(const struct position*, move m)
	attribute(nonnull);

int SEE(const struct position*, uint64_t dst, int starting_side)
	attribute(nonnull);

enum SEE_result {
	SEE_defendable,
	SEE_non_defendable,
	SEE_ok
};

int search_negative_SEE(const struct position*, int index)
	attribute(nonnull);

#endif
