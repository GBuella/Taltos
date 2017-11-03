/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2017, Gabor Buella
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALTOS_STR_UTIL_H
#define TALTOS_STR_UTIL_H

#include "chess.h"

namespace taltos
{

char index_to_file_ch(int);
char index_to_rank_ch(int, enum player);
char *print_index(char[], int index, enum player player);
const char *index_to_str(int index, enum player turn);
char piece_to_char(int);
const char *piece_name(int);
const char *piece_name_plural(int);
char square_to_char(int piece, enum player player);
const char *square_to_str_ascii(int piece, int player);
const char *square_to_str_unicode(int piece, int player);
const char *square_to_str(int piece, int player, bool use_unicode);
char *print_square(char*, int piece, enum player, bool use_unicode);
int char_to_piece(char);
bool is_file(char);
bool is_rank(char);
bool is_coordinate(const char*);
int char_to_file(char);
int char_to_rank(char, enum player turn);
int str_to_index(const char[], enum player turn);
bool empty_line(const char*);
const char *next_token(const char*);
int print_nice_number(uintmax_t, const char **postfixes, const uintmax_t *dividers);
int print_nice_count(uintmax_t);
int print_nice_ns(uintmax_t, bool use_unicode);
void board_print(char[], const struct position*, enum player turn, bool use_unicode);

}

#endif
