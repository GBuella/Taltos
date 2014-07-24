
#ifndef _PERFT_H
#define _PERFT_H

#include "chess.h"

struct divide_info;

unsigned long perft(const struct position *, unsigned depth);
unsigned long perft_ordered(const struct position *, unsigned depth);
unsigned long perft_distinct(const struct position *, unsigned depth);
struct divide_info *
divide_init(const struct position *, unsigned depth, player turn, bool ordered);
const char *divide(struct divide_info *, move_notation_type);
void divide_destruct(struct divide_info *);

#endif

