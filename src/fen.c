
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

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
#include "hash.h"
#include "util.h"

const char *start_position_fen =
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/*
 * Utility functions for parsing/printing FEN strings:
 *
 * "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR   w   KQkq   c6   0   2"
 *    |                                               |   |      |    |   |
 *  FEN_parse_board              FEN_parse_side_to_move   |      |    |   |
 *  FEN_print_board              FEN_print_side_to_move   |      |    |   |
 *                                                        |      |    |   |
 *                                  FEN_parse_castle_rights      |    |   |
 *                                  FEN_print_castle_rights      |    |   |
 *                                                               |    |   |
 *                                             FEN_parse_ep_target    |   |
 *                                             FEN_print_ep_target    |   |
 *                                                                    |   |
 *                                                            half_move   |
 *                                                                        |
 *                                                                full_move
 */

static int
coor_str_to_index(const char *str, enum player turn, jmp_buf jb)
{
	if (!is_file(str[0]) || !is_rank(str[1])) longjmp(jb, 1);

	return str_to_index(str, turn);
}

static char*
FEN_print_board(const struct position *pos, char *str, enum player turn)
{
	int empty_count;

	for (int rank = rank_8; ; rank += RSOUTH) {
		empty_count = 0;
		for (int file = file_a; is_valid_file(file); file += EAST) {
			int i = ind(rank, file);
			if (turn == black)
				i = flip_i(i);
			enum piece p = position_piece_at(pos, i);
			enum player player = position_player_at(pos, i);
			if (p != nonpiece) {
				if (empty_count > 0) {
					*str++ = '0' + (char)empty_count;
					empty_count = 0;
				}
				if (turn == black)
					player = opponent_of(player);
				*str++ = square_to_char(p, player);
			}
			else {
				++empty_count;
			}
		}
		if (empty_count > 0)
			*str++ = '0' + empty_count;
		if (rank == rank_1)
			return str;
		else
			*str++ = '/';
	}
}

static char*
FEN_print_castle_rights(const struct position *pos, char *str, enum player turn)
{
	char *c = str;

	if (turn == white) {
		if (position_cr_king_side(pos)) *c++ = 'K';
		if (position_cr_queen_side(pos)) *c++ = 'Q';
		if (position_cr_opponent_king_side(pos)) *c++ = 'k';
		if (position_cr_opponent_queen_side(pos)) *c++ = 'q';
	}
	else {
		if (position_cr_opponent_king_side(pos)) *c++ = 'K';
		if (position_cr_opponent_queen_side(pos)) *c++ = 'Q';
		if (position_cr_king_side(pos)) *c++ = 'k';
		if (position_cr_queen_side(pos)) *c++ = 'q';
	}
	if (c == str) *c++ = '-';
	return c;
}

static char*
FEN_print_side_to_move(char *str, enum player turn)
{
	if (turn == white)
		*str++ = 'w';
	else
		*str++ = 'b';
	return str;
}

static char*
FEN_print_ep_target(char *str, int ep_index, enum player turn)
{
	if (ep_index != 0)
		str = index_to_str(str, ep_index + NORTH, turn);
	else
		*str++ = '-';

	return str;
}

char*
position_print_fen(const struct position *pos,
		char str[static FEN_BUFFER_LENGTH],
		int ep_index,
		enum player turn)
{
	str = FEN_print_board(pos, str, turn);
	*str++ = ' ';
	str = FEN_print_side_to_move(str, turn);
	*str++ = ' ';
	str = FEN_print_castle_rights(pos, str, turn);
	*str++ = ' ';

	if (ep_index == 0 && position_has_en_passant_index(pos))
		ep_index = position_get_en_passant_index(pos);

	str = FEN_print_ep_target(str, ep_index, turn);
	*str = '\0';

	return str;
}

char*
position_print_fen_full(const struct position *pos,
		char str[static FEN_BUFFER_LENGTH],
		int ep_target,
		unsigned full_move,
		unsigned half_move,
		enum player turn)
{
	str = position_print_fen(pos, str, ep_target, turn);
	str += sprintf(str, " %u %u", half_move, full_move);
	return str;
}

static attribute(nonnull) const char*
read_pos_rank(char board[static 64], int rank, const char *str, jmp_buf jb)
{
	int file = file_a;

	for (int i = 0; i < 8; ++str) {
		if (*str >= '1' && *str <= '8') {
			int delta = *str - '0';

			if (i + delta > 8) longjmp(jb, 1);
			i += delta;
			file += FEAST * delta;
		}
		else {
			enum piece p = char_to_piece(*str);

			if (!is_valid_piece(p)) longjmp(jb, 1);
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

static attribute(nonnull) const char*
FEN_parse_board(char board[static 64], const char *str, jmp_buf jb)
{
	int rank = rank_7;

	memset(board, 0, 64);
	str = read_pos_rank(board, rank_8, str, jb);
	for (int i = 1; i < 8; ++i) {
		if (*str != '/') longjmp(jb, 1);
		str = read_pos_rank(board, rank, str + 1, jb);
		rank += RSOUTH;
	}
	if (!isspace((unsigned char)*str)) longjmp(jb, 1);
	return str;
}

static attribute(nonnull) const char*
FEN_parse_side_to_move(enum player *turn, const char *str, jmp_buf jb)
{
	if (strspn(str, "wWbB") != 1 || !isspace((unsigned char)str[1])) {
		longjmp(jb, 1);
	}
	*turn = (tolower((unsigned char)*str) == 'w') ? white : black;
	return ++str;
}

static attribute(nonnull) const char*
FEN_parse_castle_rights(bool rights[static 4], const char *str, jmp_buf jb)
{
	rights[0] = rights[1] = rights[2] = rights[3] = false;
	if (str[0] == '-') {
		if (!isspace((unsigned char)*++str)) longjmp(jb, 1);
		return str;
	}
	for (; !isspace((unsigned char)*str); ++str) {
		switch (*str) {
			case 'K':
				if (rights[cri_king_side])
					longjmp(jb, 1);
				rights[cri_king_side] = true;
				break;
			case 'Q':
				if (rights[cri_queen_side])
					longjmp(jb, 1);
				rights[cri_queen_side] = true;
				break;
			case 'k':
				if (rights[cri_opponent_king_side])
					longjmp(jb, 1);
				rights[cri_opponent_king_side] = true;
				break;
			case 'q':
				if (rights[cri_opponent_queen_side])
					longjmp(jb, 1);
				rights[cri_opponent_queen_side] = true;
				break;
			default:
				longjmp(jb, 1);
		}
	}
	return str;
}

static bool
is_valid_ep_pos(const char c, enum player turn)
{
	return ((c == '6' && turn == white)
	    || (c == '3' && turn == black));
}

static attribute(nonnull, returns_nonnull) const char*
FEN_parse_ep_target(int *ep_pos, const char *str, enum player turn, jmp_buf jb)
{
	if (*str == '-') {
		*ep_pos = 0;
		++str;
	}
	else {
		*ep_pos = coor_str_to_index(str, turn, jb) + 8;
		if (!is_valid_ep_pos(str[1], turn))
			longjmp(jb, 1);
		if (turn == black)
			*ep_pos = flip_i(*ep_pos);
		str += 2;
	}
	if (*str != '\0' && !isspace((unsigned char)*str))
		longjmp(jb, 1);
	return str;
}

static const char*
skip_space(const char *str)
{
	while (isspace((unsigned char)*str)) ++str;
	return str;
}

static attribute(nonnull, returns_nonnull) const char*
read_move_counter(unsigned *n, const char *str, jmp_buf jb)
{
	char *endptr;
	unsigned long l;

	l = strtoul(str, &endptr, 10);
	if (l > USHRT_MAX || endptr == str) longjmp(jb, 1);
	*n = (unsigned)l;
	return endptr;
}

const char*
read_fen_move_counters(const char *volatile str,
			unsigned *full_move,
			unsigned *half_move,
			jmp_buf jb)
{
	if (*str != '\0') {
		str = read_move_counter(half_move, str, jb);
		str = read_move_counter(full_move, skip_space(str), jb);
		if (*full_move == 0 || ((*full_move * 2) < *half_move))
			longjmp(jb, 1);
		if (!isspace((unsigned char)*str) && *str != '\0')
			longjmp(jb, 1);
	}
	else {
		*half_move = 0;
		*full_move = 1;
	}
	return str;
}

static void
flip_board(char board[static 64])
{
	for (int i = 0; i < 32; ++i) {
		char t = board[i];

		board[i] = board[flip_i(i)];
		board[flip_i(i)] = t;
	}
	for (int i = 0; i < 64; ++i) {
		if (board[i] != 0)
			board[i] ^= 1;
	}
}

static void
flip_castle_rights(bool rights[static 4])
{
	bool t = rights[cri_king_side];
	rights[cri_king_side] = rights[cri_opponent_king_side];
	rights[cri_opponent_king_side] = t;
	t = rights[cri_queen_side];
	rights[cri_queen_side] = rights[cri_opponent_queen_side];
	rights[cri_opponent_queen_side] = t;
}

static attribute(nonnull(2)) const char*
read_fen(struct position *pos,
	const char *volatile str,
	int *ep_index,
	enum player *turn,
	jmp_buf jb)
{
	enum player t;
	char board[64];
	bool castle_rights[4];
	int dummy_ep;

	if (turn == NULL)
		turn = &t;
	if (ep_index == NULL)
		ep_index = &dummy_ep;

	str = FEN_parse_board(board, skip_space(str), jb);
	str = FEN_parse_side_to_move(turn, skip_space(str), jb);
	str = FEN_parse_castle_rights(castle_rights, skip_space(str), jb);
	str = FEN_parse_ep_target(ep_index, skip_space(str), *turn, jb);

	if (*turn == black) {
		flip_board(board);
		flip_castle_rights(castle_rights);
		if (*ep_index != 0)
			*ep_index = flip_i(*ep_index);
	}
	if (position_reset(pos, board, castle_rights, *ep_index) != 0)
		longjmp(jb, -1);
	return str;
}

const char*
position_read_fen(struct position *pos,
		const char *volatile str,
		int *ep_index,
		enum player *turn)
{
	jmp_buf jb;

	if (setjmp(jb) != 0)
		return NULL;
	return read_fen(pos, str, ep_index, turn, jb);
}

const char*
position_read_fen_full(struct position *pos,
			const char *volatile str,
			int *ep_index,
			unsigned *full_move,
			unsigned *half_move,
			enum player *turn)
{
	jmp_buf jb;

	if (setjmp(jb) != 0)
		return NULL;
	str = skip_space(read_fen(pos, str, ep_index, turn, jb));
	str = read_fen_move_counters(str, full_move, half_move, jb);
	return str;
}
