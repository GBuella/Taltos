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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "macros.h"
#include "bitboard.h"
#include "constants.h"
#include "chess.h"
#include "position.h"
#include "str_util.h"

static char*
print_san_move_from(const struct position *pos, struct move m, char *str)
{
	struct move moves[MOVE_ARRAY_LENGTH];
	enum piece p = pos_piece_at(pos, m.from);

	if ((p == pawn) && is_capture(m)) {
		*str++ = index_to_file_ch(m.from);
		return str;
	}

	bool rank_is_ambiguous = false;
	bool file_is_ambiguous = false;

	(void) gen_moves(pos, moves);
	for (struct move *im = moves; !is_null_move(*im); ++im) {
		if (im->from == m.from)
			continue;
		if ((im->to == m.to) && (pos_piece_at(pos, im->from) == p)) {
			if (ind_rank(im->from) == ind_rank(m.from))
				rank_is_ambiguous = true;
			if (ind_file(im->from) == ind_file(m.from))
				file_is_ambiguous = true;
		}
	}

	if (file_is_ambiguous)
		*str++ = index_to_file_ch(m.from);

	if (rank_is_ambiguous)
		*str++ = index_to_rank_ch(m.from);

	return str;
}

static char*
print_san_promotion(char *str, struct move m)
{
	if (is_promotion(m)) {
		*str++ = '=';
		*str++ = toupper((unsigned char)piece_to_char(m.result));
	}

	return str;
}

static char*
print_san_check(char *str, const struct position *pos, struct move m)
{
	struct position child;
	make_move(&child, pos, m);
	if (is_in_check(&child)) {
		struct move moves[MOVE_ARRAY_LENGTH];
		if (gen_moves(&child, moves) == 0)
			*str++ = '#';
		else
			*str++ = '+';
	}

	return str;
}

char*
print_san_move_internal(char *str, const struct position *pos, struct move m,
			bool use_unicode)
{
	enum piece piece = pos_piece_at(pos, m.from);

	if (m.type == mt_castle_kingside)
		return str + sprintf(str, "O-O");
	else if (m.type == mt_castle_queenside)
		return str + sprintf(str, "O-O-O");

	if (piece != pawn)
		str = print_square(str, piece, white, use_unicode);

	str = print_san_move_from(pos, m, str);

	if (is_capture(m))
		*str++ = 'x';

	str = print_index(str, m.to);

	if (m.type == mt_en_passant)
		return str + sprintf(str, "e.p.");

	str = print_san_promotion(str, m);
	str = print_san_check(str, pos, m);
	*str = '\0';
	return str;
}

char*
print_san_move(char *str, const struct position *pos, struct move m)
{
	return print_san_move_internal(str, pos, m, false);
}

char*
print_fan_move(char *str, const struct position *pos, struct move m)
{
	return print_san_move_internal(str, pos, m, true);
}

char*
print_coor_move(char *str, struct move m)
{
	str = print_index(str, m.from);
	str = print_index(str, m.to);
	if (is_promotion(m))
		*str++ = tolower((unsigned char)piece_to_char(m.result));
	*str = '\0';

	return str;
}

char*
print_move(char str[static MOVE_STR_BUFFER_LENGTH],
	   const struct position *pos,
	   struct move m,
	   enum move_notation_type t)
{
	assert(is_move_valid(m));
	assert(is_valid_piece(pos_piece_at(pos, m.from)));

	switch (t) {
	case mn_coordinate:
		return print_coor_move(str, m);
		break;
	case mn_fan:
		return print_fan_move(str, pos, m);
		break;
	default:
	case mn_san:
		return print_san_move(str, pos, m);
		break;
	}
}

int
fen_read_move(const char *fen, const char *str, struct move *m)
{
	struct move t;
	struct position position[1];

	if (m == NULL)
		m = &t;

	if (position_read_fen(fen, position) == NULL)
		return 1;

	return read_move(position, str, m);
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
		else if (is_rank_char(*src) || is_file_char(*src))
			*dst++ = *src;
		else if (tolower(*src) == 'o' || *src == '-')
			*dst++ = toupper(*src);
	} while (*++src != '\0');

	*dst = '\0';
}

int
read_move(const struct position *pos, const char *move_str, struct move *m)
{
	struct move moves[MOVE_ARRAY_LENGTH];
	char str[8] = {0, };

	if (move_str[0] == '\0')
		return 1;

	strncpy(str, move_str, sizeof(str) - 1);
	cleanup_move(str);
	(void) gen_moves(pos, moves);
	for (struct move *mp = moves; !is_null_move(*mp); ++mp) {
		char tstr[MOVE_STR_BUFFER_LENGTH];

		(void) print_coor_move(tstr, *mp);
		if (strcmp(str, tstr) == 0) {
			*m = *mp;
			return 0;
		}
		(void) print_san_move(tstr, pos, *mp);
		cleanup_move(tstr);
		if (strcmp(str, tstr) == 0) {
			*m = *mp;
			return 0;
		}
	}

	return 1;
}
