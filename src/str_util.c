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
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "macros.h"
#include "str_util.h"

char
index_to_file_ch(coordinate index)
{
	assert(ivalid(index));
	return (char)('h' - ind_file(index));
}

char
index_to_rank_ch(coordinate index)
{
	assert(ivalid(index));
	int rank = ind_rank(index);
	return '8' - rank;
}

char*
print_index(char *str, coordinate index)
{
	if (str == NULL)
		return NULL;

	*str++ = index_to_file_ch(index);
	*str++ = index_to_rank_ch(index);
	return str;
}

const char*
index_to_str(coordinate index)
{
	static const char strs[64][3] = {
		"h8", "g8", "f8", "e8", "d8", "c8", "b8", "a8",
		"h7", "g7", "f7", "e7", "d7", "c7", "b7", "a7",
		"h6", "g6", "f6", "e6", "d6", "c6", "b6", "a6",
		"h5", "g5", "f5", "e5", "d5", "c5", "b5", "a5",
		"h4", "g4", "f4", "e4", "d4", "c4", "b4", "a4",
		"h3", "g3", "f3", "e3", "d3", "c3", "b3", "a3",
		"h2", "g2", "f2", "e2", "d2", "c2", "b2", "a2",
		"h1", "g1", "f1", "e1", "d1", "c1", "b1", "a1"
	};

	return strs[index];
}

char
piece_to_char(enum piece p)
{
	switch (p) {
	case pawn: return 'p';
	case rook: return 'r';
	case knight: return 'n';
	case bishop: return 'b';
	case queen: return 'q';
	case king: return 'k';
	case 0: return ' ';
	default: assert(0); return ' ';
	}
}

const char*
piece_name(enum piece p)
{
	switch (p) {
	case pawn: return "pawn";
	case rook: return "rook";
	case knight: return "knight";
	case bishop: return "bishop";
	case queen: return "queen";
	case king: return "king";
	case 0: return " ";
	default: assert(0); return " ";
	}
}

const char*
piece_name_plural(enum piece p)
{
	switch (p) {
	case pawn: return "pawns";
	case rook: return "rooks";
	case knight: return "knights";
	case bishop: return "bishops";
	case queen: return "queens";
	case king: return "kings";
	case 0: return " ";
	default: assert(0); return " ";
	}
}

const char*
square_to_str_ascii(enum piece p, enum player pl)
{
	if (pl == white) {
		switch (p) {
		case pawn:
			return "P";
		case knight:
			return "N";
		case bishop:
			return "B";
		case rook:
			return "R";
		case queen:
			return "Q";
		case king:
			return "K";
		case nonpiece:
			return " ";
		default:
			assert(false);
			return " ";
		}
	}
	else {
		switch (p) {
		case pawn:
			return "p";
		case knight:
			return "n";
		case bishop:
			return "b";
		case rook:
			return "r";
		case queen:
			return "q";
		case king:
			return "k";
		case nonpiece:
			return " ";
		default:
			assert(false);
			return " ";
		}
	}

}

char
piece_player_to_char(enum piece p, enum player pl)
{
	char pc = piece_to_char(p);

	return (pl == black) ? pc : (char)toupper((unsigned char)pc);
}

char
square_to_char(int square)
{
	return piece_player_to_char(square_piece(square),
				    square_player(square));
}

const char*
square_to_str_unicode(enum piece p, enum player pl)
{
	if (pl == white) {
		switch (p) {
		case pawn:
			return "\U00002659";
		case knight:
			return "\U00002658";
		case bishop:
			return "\U00002657";
		case rook:
			return "\U00002656";
		case queen:
			return "\U00002655";
		case king:
			return "\U00002654";
		case nonpiece:
			return " ";
		default:
			assert(0);
			return " ";
		}
	}
	else {
		switch (p) {
		case pawn:
			return "\U0000265f";
		case knight:
			return "\U0000265e";
		case bishop:
			return "\U0000265d";
		case rook:
			return "\U0000265c";
		case queen:
			return "\U0000265b";
		case king:
			return "\U0000265a";
		case nonpiece:
			return " ";
		default:
			assert(0);
			return " ";
		}
	}
}

const char*
square_to_str(enum piece p, enum player pl, bool use_unicode)
{
	if (use_unicode)
		return square_to_str_unicode(p, pl);
	else
		return square_to_str_ascii(p, pl);
}

char*
print_square(char *buf, enum piece p, enum player pl, bool use_unicode)
{
	const char *str = square_to_str(p, pl, use_unicode);

	strcpy(buf, str);
	return buf + strlen(str);
}

enum piece
char_to_piece(char p)
{
	switch (tolower((unsigned char)p)) {
	case 'p': return pawn;
	case 'r': return rook;
	case 'b': return bishop;
	case 'n': return knight;
	case 'q': return queen;
	case 'k': return king;
	default: return 0;
	}
}

bool
is_file_char(char c)
{
	return (((c >= 'a') && (c <= 'h'))
	    || ((c >= 'A') && (c <= 'H')));
}

bool
is_rank_char(char c)
{
	return ((c >= '1') && (c <= '8'));
}

bool
is_coordinate(const char *str)
{
	assert(str != NULL);
	return is_file_char(str[0]) && is_rank_char(str[1]);
}

file
char_to_file(char ch)
{
	assert(is_file_char(ch));
	return 7 - (toupper((unsigned char)ch) - 'A');
}

rank
char_to_rank(char ch)
{
	assert(is_rank_char(ch));
	return 7 - (ch - '1');
}

coordinate
str_to_index(const char str[static 2])
{
	return ind(char_to_rank(str[1]), char_to_file(str[0]));
}

const char*
next_token(const char *str)
{
	static const char separator[] = " \t";
	static const char line_separator[] = "\n\r";

	if (strchr(separator, *str) != NULL) {
		str += strspn(str, separator);
		if (strchr(line_separator, *str) == NULL) {
			return str;
		}
		else {
			return NULL;
		}
	}
	if (strchr(line_separator, *str) == NULL)
		return next_token(str + strcspn(str, separator));
	else
		return NULL;
}

bool
empty_line(const char *line)
{
	return next_token(line) == NULL;
}

int
print_nice_number(uintmax_t n, const char **postfix, const uintmax_t *divider)
{
	const struct lconv *lconv = localeconv();

	while (divider[1] != 0 && (*divider > n || divider[1] <= n)) {
		++postfix;
		++divider;
	}
	if (*divider == 1)
		return printf("%ju%s", n, *postfix);
	n /= ((*divider) / 10);
	if (n % 10 == 0)
		return printf("%ju%s", n / 10, *postfix);
	else
		return printf("%ju%s%ju%s",
		    n / 10, lconv->decimal_point, n % 10, *postfix);
}

int
print_nice_count(uintmax_t n)
{
	/* BEGIN CSTYLED */ /* cstyle does not know about this syntax */
	return print_nice_number(n,
	    (const char *[]) {"", "k", "m", "g", NULL},
	    (const uintmax_t[]) {1, 1000, MILLION, BILLION, 0});
	/* END CSTYLED */
}

int
print_nice_ns(uintmax_t n, bool use_unicode)
{
	static const char *ascii_postfixes[] = {"ns", "s", NULL};
	static const uintmax_t ascii_dividers[] = {1, BILLION, 0};

	static const char *unicode_postfixes[] =
	    {"ns", U_MU "s", "ms", "s", NULL};
	static const uintmax_t unicode_dividers[] =
	    {1, 1000, MILLION, BILLION, 0};

	return print_nice_number(n,
	    use_unicode ? unicode_postfixes : ascii_postfixes,
	    use_unicode ? unicode_dividers : ascii_dividers);
}

void
board_print(char str[static BOARD_BUFFER_LENGTH],
		const struct position *pos,
		bool use_unicode,
		bool white_on_top)
{
	for (int rank = rank_8; is_valid_rank(rank); rank += RSOUTH) {
		for (int file = file_a; is_valid_file(file); file += EAST) {
			int index = ind(rank, file);
			if (white_on_top)
				index = flip_i(index);
			enum piece p = position_piece_at(pos, index);
			enum player pl = position_player_at(pos, index);

			if (use_unicode)
				*str++ = ' ';

			str = print_square(str, p, pl, use_unicode);
		}
		*str++ = '\n';
	}
	*str++ = '\0';
}
