/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2018, Gabor Buella
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "chess.h"
#include "constants.h"
#include "str_util.h"
#include "util.h"

const char *start_position_fen =
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/*
 * Utility functions for parsing/printing FEN strings:
 *
 * "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR   w   KQkq   c6   0   2"
 *        |                                           |   |      |    |   |
 *      parse_board                  parse_side_to_move   |      |    |   |
 *      print_board                  print_side_to_move   |      |    |   |
 *                                                        |      |    |   |
 *                                      parse_castle_rights      |    |   |
 *                                      print_castle_rights      |    |   |
 *                                                               |    |   |
 *                                                 parse_ep_target    |   |
 *                                                 print_ep_target    |   |
 *                                                                    |   |
 *                                                            half_move   |
 *                                                                        |
 *                                                                full_move
 */

static char*
print_board(char *str, const struct position *pos)
{
	int empty_count;

	for (int rank = rank_8; ; rank += RSOUTH) {
		empty_count = 0;
		for (int file = file_a; is_valid_file(file); file += EAST) {
			int square = position_square_at(pos, ind(rank, file));
			if (square != 0) {
				if (empty_count > 0) {
					*str++ = '0' + (char)empty_count;
					empty_count = 0;
				}
				*str++ = square_to_char(square);
			}
			else {
				++empty_count;
			}
		}
		if (empty_count > 0)
			*str++ = '0' + (char)empty_count;
		if (rank == rank_1)
			return str;
		else
			*str++ = '/';
	}
}

static char*
print_castle_rights(char *str, const struct position *pos)
{
	char *c = str;

	if (position_cr_white_king_side(pos))
		*c++ = 'K';
	if (position_cr_white_queen_side(pos))
		*c++ = 'Q';
	if (position_cr_black_king_side(pos))
		*c++ = 'k';
	if (position_cr_black_queen_side(pos))
		*c++ = 'q';
	if (c == str)
		*c++ = '-';

	return c;
}

static char*
print_side_to_move(char *str, const struct position *pos)
{
	if (position_turn(pos) == white)
		*str++ = 'w';
	else
		*str++ = 'b';

	return str;
}

char*
print_ep_target(char *str, const struct position *pos)
{
	if (position_has_en_passant_target(pos))
		str = print_index(str, position_get_en_passant_target(pos));
	else
		*str++ = '-';

	return str;
}

char*
position_print_fen_no_move_count(char str[static FEN_BUFFER_LENGTH],
		   const struct position *pos)
{
	str = print_board(str, pos);
	*str++ = ' ';
	str = print_side_to_move(str, pos);
	*str++ = ' ';
	str = print_castle_rights(str, pos);
	*str++ = ' ';
	str = print_ep_target(str, pos);

	*str = '\0';
	return str;
}


char*
position_print_fen(char str[static FEN_BUFFER_LENGTH],
		   const struct position *pos)
{
	str = position_print_fen_no_move_count(str, pos);
	str += sprintf(str, " %u", position_half_move_count(pos));
	str += sprintf(str, " %u", position_full_move_count(pos));

	return str;
}

static const char*
read_pos_rank(char board[static 64], int rank, const char *str)
{
	int file = file_a;

	for (int i = 0; i < 8; ++str) {
		if (*str >= '1' && *str <= '8') {
			int delta = *str - '0';

			if (i + delta > 8)
				return NULL;
			i += delta;
			file += FEAST * delta;
		}
		else {
			enum piece p = char_to_piece(*str);

			if (!is_valid_piece(p))
				return NULL;
			if (isupper((unsigned char)*str)) {
				board[ind(rank, file)] = p | white;
			}
			else {
				board[ind(rank, file)] = p | black;
			}
			file += FEAST;
			++i;
		}
	}
	return str;
}

static const char*
parse_board(const char *str, char board[static 64])
{
	int rank = rank_7;

	if (str == NULL)
		return NULL;

	memset(board, 0, 64);

	if ((str = read_pos_rank(board, rank_8, str)) == NULL)
		return NULL;

	for (int i = 1; i < 8; ++i) {
		if (*str != '/')
			return NULL;
		if ((str = read_pos_rank(board, rank, str + 1)) == NULL)
			return NULL;
		rank += RSOUTH;
	}

	if (!isspace((unsigned char)*str))
		return NULL;

	return str;
}

static const char*
parse_side_to_move(const char *str, enum player *turn)
{
	if (str == NULL)
		return NULL;

	if (tolower((unsigned char)*str) == 'w')
		*turn = white;
	else if (tolower((unsigned char)*str) == 'b')
		*turn = black;
	else
		return NULL;

	++str;

	if (!isspace((unsigned char)*str))
		return NULL;

	return str;
}

static const char*
parse_castle_rights(const char *str, bool rights[static 4])
{
	if (str == NULL)
		return NULL;

	rights[0] = rights[1] = rights[2] = rights[3] = false;

	if (str[0] == '-') {
		++str;
		if (!isspace((unsigned char)*str))
			return false;
		return str;
	}

	for (; !isspace((unsigned char)*str); ++str) {
		switch (*str) {
			case 'K':
				if (rights[cri_white_king_side])
					return NULL;
				rights[cri_white_king_side] = true;
				break;
			case 'Q':
				if (rights[cri_white_queen_side])
					return NULL;
				rights[cri_white_queen_side] = true;
				break;
			case 'k':
				if (rights[cri_black_king_side])
					return NULL;
				rights[cri_black_king_side] = true;
				break;
			case 'q':
				if (rights[cri_black_queen_side])
					return NULL;
				rights[cri_black_queen_side] = true;
				break;
			default:
				return NULL;
		}
	}

	return str;
}

static bool
is_valid_ep_index(int index, enum player turn)
{
	if (turn == white)
		return ind_rank(index) == rank_5;
	else
		return ind_rank(index) == rank_4;
}

static const char*
parse_ep_target(const char *str, coordinate *ep_target, enum player turn)
{
	if (str == NULL)
		return NULL;

	if (*str == '-') {
		*ep_target = 0;
		++str;
	}
	else {
		if (!is_file_char(str[0]) || !is_rank_char(str[1]))
			return NULL;
    *ep_target = str_to_index(str) + (turn == white ? SOUTH : NORTH);
		if (!is_valid_ep_index(*ep_target, turn))
			return NULL;
		str += 2;
	}

	if (*str != '\0' && !isspace((unsigned char)*str))
		return NULL;

	return str;
}

static const char*
skip_space(const char *str)
{
	if (str == NULL)
		return NULL;

	while (isspace((unsigned char)*str))
		++str;

	return str;
}

static const char*
read_move_counter(const char *str, unsigned *n)
{
	char *endptr;
	unsigned long l;

	if (str == NULL)
		return NULL;

	l = strtoul(str, &endptr, 10);
	if (l > USHRT_MAX || endptr == str)
		return NULL;
	*n = (unsigned)l;
	return endptr;
}

const char*
position_read_fen(const char *str, struct position *pos)
{
	struct position_desc desc;

	str = skip_space(str);
	str = parse_board(str, desc.board);
	str = skip_space(str);
	str = parse_side_to_move(str, &desc.turn);
	str = skip_space(str);
	str = parse_castle_rights(str, desc.castle_rights);
	str = skip_space(str);
	str = parse_ep_target(str, &desc.en_passant_index, desc.turn);
	str = skip_space(str);

	if (str == NULL)
		return NULL;

	if (isdigit(*str)) {
		str = read_move_counter(str, &desc.half_move_counter);
		str = skip_space(str);
		str = read_move_counter(str, &desc.full_move_counter);
		if (str == NULL)
			return NULL;

		/*
		 * Normally Taltos would check this:
		 * (*full_move_counter * 2) < *half_move_counter
		 *
		 * But apparently some GUIs out there don't care about this,
		 * and when presented with a FEN such as:
		 *
		 * 8/1k6/3p4/p2P1p2/P2P1P2/8/8/1K6 w - - 99 123
		 *
		 * They scrap the full move counter, and pass on this to
		 * the engine:
		 *
		 * 8/1k6/3p4/p2P1p2/P2P1P2/8/8/1K6 w - - 99 1
		 */
		if (desc.full_move_counter == 0)
			return NULL;

		if (!isspace((unsigned char)*str) && *str != '\0')
			return NULL;
	}
	else {
		desc.half_move_counter = 0;
		desc.full_move_counter = 1;
	}

	if (position_reset(pos, desc) != 0)
		return NULL;

	return str;
}
