
#ifndef STR_UTIL_H
#define STR_UTIL_H

#include <stdbool.h>

#include "chess.h"

char index_to_file_ch(int);
char index_to_rank_ch(int, player);
char *index_to_str(char[static 2], int, player);
char piece_to_char(piece);
char square_to_char(piece, player);
piece char_to_piece(char);
bool is_file(char);
bool is_rank(char);
bool is_coordinate(const char *);
file_t char_to_file(char);
rank_t char_to_rank(char, player turn);
int str_to_index(const char[static 2], player turn);

#endif
