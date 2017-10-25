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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "move.h"
#include "bitboard.h"
#include "constants.h"
#include "chess.h"
#include "position.h"
#include "str_util.h"

namespace taltos
{

static char*
print_san_move_from(const struct position *pos, move m,
			char *str, enum player turn)
{
	move moves[MOVE_ARRAY_LENGTH];
	uint64_t ambig_pieces = UINT64_C(0);
	enum piece p = pos_piece_at(pos, mfrom(m));

	(void) gen_moves(pos, moves);
	for (move *im = moves; *im != 0; ++im) {
		if ((mfrom(*im) != mfrom(m)) && (mto(*im) == mto(m))
		    && (pos_piece_at(pos, mfrom(*im)) == p))
			ambig_pieces |= mfrom64(*im);
	}
	if ((p == pawn) && is_capture(m)) {
		*(str++) = index_to_file_ch(mfrom(m));
	}
	else if (is_nonempty(ambig_pieces)) {
		if (is_nonempty(ambig_pieces & file64(mfrom(m)))) {
			if (is_nonempty(ambig_pieces & rank64(mfrom(m)))) {
				*(str++) = index_to_file_ch(mfrom(m));
			}
			*(str++) = index_to_rank_ch(mfrom(m), turn);
		}
		else {
			*(str++) = index_to_file_ch(mfrom(m));
		}
	}
	return str;
}

static char*
print_san_promotion(move m, char *str)
{
	if (is_promotion(m)) {
		*str++ = '=';
		*str++ = toupper((unsigned char)piece_to_char(mresultp(m)));
	}
	return str;
}

static char*
print_san_check(const struct position *pos, move m, char *str)
{
	struct position child;
	make_move(&child, pos, m);
	if (is_in_check(&child)) {
		move moves[MOVE_ARRAY_LENGTH];
		if (gen_moves(&child, moves) == 0)
			*str++ = '#';
		else
			*str++ = '+';
	}

	return str;
}

char*
print_san_move_internal(const struct position *pos, move m, char *str,
			enum player turn, bool use_unicode)
{
	int piece = pos_piece_at(pos, mfrom(m));

	if (mtype(m) == mt_castle_kingside)
		return str + sprintf(str, "O-O");
	else if (mtype(m) == mt_castle_queenside)
		return str + sprintf(str, "O-O-O");

	if (piece != pawn)
		str = print_square(str, piece, white, use_unicode);

	str = print_san_move_from(pos, m, str, turn);

	if (is_capture(m))
		*str++ = 'x';

	str = print_index(str, mto(m), turn);

	if (mtype(m) == mt_en_passant)
		return str + sprintf(str, "e.p.");

	str = print_san_promotion(m, str);
	str = print_san_check(pos, m, str);
	*str = '\0';
	return str;
}

char*
print_san_move(const struct position *pos, move m, char *str, enum player turn)
{
	return print_san_move_internal(pos, m, str, turn, false);
}

char*
print_fan_move(const struct position *pos, move m, char *str,
		       enum player turn)
{
	return print_san_move_internal(pos, m, str, turn, true);
}

char*
print_coor_move(move m, char str[static MOVE_STR_BUFFER_LENGTH],
		enum player turn)
{
	str = print_index(str, mfrom(m), turn);
	str = print_index(str, mto(m), turn);
	if (is_promotion(m))
		*str++ = tolower((unsigned char)piece_to_char(mresultp(m)));
	*str = '\0';
	return str;
}

char*
print_move(const struct position *pos, move m,
		char str[static MOVE_STR_BUFFER_LENGTH],
		enum move_notation_type t,
		enum player turn)
{
	assert(is_move_valid(m));
	assert(is_valid_piece(pos_piece_at(pos, mfrom(m))));

	switch (t) {
	case mn_coordinate:
		return print_coor_move(m, str, turn);
		break;
	case mn_fan:
		return print_fan_move(pos, m, str, turn);
		break;
	default:
	case mn_san:
		return print_san_move(pos, m, str, turn);
		break;
	}
}

int
fen_read_move(const char *fen, const char *str, move *m)
{
	move t;
	enum player turn;
	struct position position[1];

	if (m == NULL)
		m = &t;
	if (position_read_fen(position, fen, NULL, &turn) == NULL)
		return -1;
	return read_move(position, str, m, turn);
}

static void
cleanup_move(char str[])
{
	char *src;
	char *dst;

	src = dst = str;

	// remove periods "exb3.ep" -> "exb3ep"
	do {
		if (*src != '.')
			*dst++ = *src;
		++src;
	} while (*src != '\0' && !isspace((unsigned char)*src));

	// remove "ep" "exb3ep" -> "exb3"
	if (dst - str >= 3 && dst[-1] == 'p' && dst[-2] == 'e')
		dst -= 2;
	*dst = '\0';

	// remove extra characters
	src = dst = str;
	do {
		if (strchr("KQNRBkqnrb", *src) != NULL)
			*dst++ = tolower(*src);
		else if (is_rank(*src) || is_file(*src))
			*dst++ = *src;
		else if (tolower(*src) == 'o' || *src == '-')
			*dst++ = toupper(*src);
	} while (*++src != '\0');

	*dst = '\0';
}

int
read_move(const struct position *pos, const char *move_str,
		move *m, enum player turn)
{
	move moves[MOVE_ARRAY_LENGTH];
	char str[8] = {0, };

	if (move_str[0] == '\0')
		return none_move;
	strncpy(str, move_str, sizeof(str) - 1);
	cleanup_move(str);
	(void) gen_moves(pos, moves);
	for (move *mp = moves; *mp != 0; ++mp) {
		char tstr[MOVE_STR_BUFFER_LENGTH];

		(void) print_coor_move(*mp, tstr, turn);
		if (strcmp(str, tstr) == 0) {
			*m = *mp;
			return 0;
		}
		(void) print_san_move(pos, *mp, tstr, turn);
		cleanup_move(tstr);
		if (strcmp(str, tstr) == 0) {
			*m = *mp;
			return 0;
		}
	}
	return none_move;
}

}
