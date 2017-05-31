
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

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
	setup_registers();
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

static char*
print_san_move(const struct position *pos, move m, char *str, enum player turn)
{
	int piece = pos_piece_at(pos, mfrom(m));

	if (mtype(m) == mt_castle_kingside)
		return str + sprintf(str, "O-O");
	else if (mtype(m) == mt_castle_queenside)
		return str + sprintf(str, "O-O-O");
	if (piece != pawn)
		*str++ = (char)toupper((unsigned char)piece_to_char(piece));
	str = print_san_move_from(pos, m, str, turn);
	if (is_capture(m))
		*str++ = 'x';
	str = index_to_str(str, mto(m), turn);
	if (mtype(m) == mt_en_passant)
		return str + sprintf(str, "e.p.");
	str = print_san_promotion(m, str);
	str = print_san_check(pos, m, str);
	*str = '\0';
	return str;
}

char*
print_coor_move(move m, char str[static MOVE_STR_BUFFER_LENGTH],
		enum player turn)
{
	str = index_to_str(str, mfrom(m), turn);
	str = index_to_str(str, mto(m), turn);
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
	} while (*src != '\0' && !isspace(*src));

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
	char str[8];

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
