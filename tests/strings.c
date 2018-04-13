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

#include "tests.h"

#include <string.h>

#include "str_util.h"
#include "position.h"

static void
test_chars(void)
{
	assert_int(char_to_file('a'), ==, file_a);
	assert_int(char_to_file('b'), ==, file_b);
	assert_int(char_to_file('h'), ==, file_h);
	assert_int(char_to_file('A'), ==, file_a);
	assert_int(char_to_file('B'), ==, file_b);
	assert_int(char_to_file('H'), ==, file_h);

	assert_int(char_to_rank('1'), ==, rank_1);
	assert_int(char_to_rank('2'), ==, rank_2);
	assert_int(char_to_rank('3'), ==, rank_3);
	assert_int(char_to_rank('4'), ==, rank_4);
	assert_int(char_to_rank('5'), ==, rank_5);
	assert_int(char_to_rank('8'), ==, rank_8);

	assert_char(index_to_file_ch(0), ==, 'h');
	assert_char(index_to_file_ch(1), ==, 'g');
	assert_char(index_to_file_ch(7), ==, 'a');
	assert_char(index_to_file_ch(63), ==, 'a');

	assert_char(index_to_rank_ch(0), ==, '8');
	assert_char(index_to_rank_ch(1), ==, '8');
	assert_char(index_to_rank_ch(8 + 7), ==, '7');
	assert_char(index_to_rank_ch(63), ==, '1');

	assert_char(piece_to_char(queen), ==, 'q');

	assert_char(piece_player_to_char(queen, white), ==, 'Q');
	assert_char(piece_player_to_char(queen, black), ==, 'q');
	assert_char(square_to_char(white_queen), ==, 'Q');
	assert_char(square_to_char(black_queen), ==, 'q');

	assert_true(is_file_char('f'));
	assert_true(is_file_char('F'));
	assert_false(is_file_char('4'));
	assert_false(is_file_char('i'));
	assert_false(is_file_char(' '));

	assert_true(is_rank_char('1'));
	assert_true(is_rank_char('6'));
	assert_false(is_rank_char('9'));
	assert_false(is_rank_char('0'));
	assert_false(is_rank_char('a'));
	assert_false(is_rank_char(' '));
}

static void
test_coordinates(void)
{
	assert_true(is_coordinate("g6"));
	assert_false(is_coordinate("g9"));
	assert_false(is_coordinate("g0"));
	assert_false(is_coordinate("6"));
	assert_true(is_coordinate("g6 lorem ipsum"));
	assert_false(is_coordinate("lorem ipsum"));

	assert_int(str_to_index("g6"), ==, ind(rank_6, file_g));
}

static bool
is_valid(const char *fen)
{
	struct position pos;
	return position_read_fen(fen, &pos) != NULL;
}

static void
test_invalid_fens(void)
{
	assert_false(is_valid(""));
	assert_false(is_valid(" "));
	assert_false(is_valid("asdfgh"));
	assert_false(is_valid("8/8/8"));
	assert_false(is_valid("8/8/8/8/8/8/8/8"));
	assert_false(is_valid("8/8/8/8/8/8/8/8 w - -"));

	// no black king
	assert_false(is_valid(
	    "rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -"));

	// no white king
	assert_false(is_valid(
	    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w KQkq -"));

	// two black kings
	assert_false(is_valid(
	    "rnbqkbnr/pppppppp/8/5k2/8/8/PPPPPPPP/RNBQKBNR w KQkq -"));

	// two white kings
	assert_false(is_valid(
	    "rnbqkbnr/pppppppp/8/5K2/8/8/PPPPPPPP/RNBQKBNR w KQkq -"));

	// too many black bishops
	assert_false(is_valid(
	    "rnbqkbnr/pppppppp/8/5b2/8/8/PPPP1PPP/RNBQKBNR w KQkq -"));

	// too many white queens
	assert_false(is_valid(
	    "rnbqkbnr/pppppppp/8/4QQ2/8/8/PPPPPPP1/RNBQKBNR w KQkq -"));

	// kings attacking each other
	assert_false(is_valid(
	    "rnbq1bnr/pppppppp/8/4kK2/8/8/PPPPPPP1/RNBQ1BNR w - -"));

	// opponent king is in check
	assert_false(is_valid("B6b/8/8/8/2K5/5k2/8/b6B w - -"));

	// two bishops attacking king
	assert_false(is_valid("B6b/8/8/8/2K5/5k2/8/b6B b - - 0 1"));

	// two rooks attacking king
	assert_false(is_valid("8/8/4R3/1K6/4k3/8/4Q3/8 b - - 0 1"));

	// two pawns attacking king
	assert_false(is_valid("8/8/8/1K6/4k3/3P1P2/8/8 b - - 0 1"));

	// two knights attacking king
	assert_false(is_valid("8/8/3N4/1K4N1/4k3/8/8/8 b - - 0 1"));

	// invalid castling rights
	assert_false(is_valid(
	    "1nbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w q -"));
	assert_false(is_valid(
	    "rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w k -"));
	assert_false(is_valid(
	    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/1NBQKBNR w Q -"));
	assert_false(is_valid(
	    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN1 w K -"));
	assert_false(is_valid(
	    "rnbq1bnr/pppppppp/8/1k6/8/8/PPPPPPPP/RNBQKBNR w kq -"));
	assert_false(is_valid(
	    "rnbqkbnr/pppppppp/8/1K6/8/8/PPPPPPPP/RNBQ1BNR w KQ -"));
}

static const struct {
	const char *FEN;
	const char *board;
} positions[] = {
	{"7k/8/8/1p6/P7/8/8/7K b - -",
	"       k\n"
	"        \n"
	"        \n"
	" p      \n"
	"P       \n"
	"        \n"
	"        \n"
	"       K\n"},
	{"rnbqkbnr/ppppppp1/8/5b2/5Q2/8/PPPP1PPP/RNBQKBNR w KQkq -",
	"rnbqkbnr\n"
	"ppppppp \n"
	"        \n"
	"     b  \n"
	"     Q  \n"
	"        \n"
	"PPPP PPP\n"
	"RNBQKBNR\n"},
	{"r2qkbnr/ppp1pppp/n7/3pP3/8/8/PPPP1PPP/RNBQK2R w KQkq d6 3 3",
	"r  qkbnr\n"
	"ppp pppp\n"
	"n       \n"
	"   pP   \n"
	"        \n"
	"        \n"
	"PPPP PPP\n"
	"RNBQK  R\n"},
	{"rnbq2nr/pppppppp/5k2/8/8/1P6/P1PPP1PP/RNBQK2R w KQ -",
	"rnbq  nr\n"
	"pppppppp\n"
	"     k  \n"
	"        \n"
	"        \n"
	" P      \n"
	"P PPP PP\n"
	"RNBQK  R\n"},
	{"rnbqkbnr/ppPppppp/8/8/8/8/PPPPPP1P/RNBQKBNR w KQkq -",
	"rnbqkbnr\n"
	"ppPppppp\n"
	"        \n"
	"        \n"
	"        \n"
	"        \n"
	"PPPPPP P\n"
	"RNBQKBNR\n"},
	{"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
	"rnbqkbnr\n"
	"pppppppp\n"
	"        \n"
	"        \n"
	"    P   \n"
	"        \n"
	"PPPP PPP\n"
	"RNBQKBNR\n"}
};

static void
test_fen_basic(void)
{
	struct position *position;
	struct position *next;
	struct move move = {.from = e2, .to = e4,
		.result = pawn, .captured = 0, .type = mt_pawn_double_push,};
	char str[FEN_BUFFER_LENGTH + MOVE_STR_BUFFER_LENGTH];

	position = position_allocate();
	assert_ptr_not_null(position);
	next = position_allocate();
	assert_ptr_not_null(next);
	assert_not_null(position_read_fen(start_position_fen, position));
	assert_int(position_half_move_count(position), ==, 0);
	assert_int(position_full_move_count(position), ==, 1);
	assert_int(position_turn(position), ==, white);
	assert_ptr_equal(position_print_fen(str, position),
	    str + strlen(start_position_fen));
	assert_string_equal(str, start_position_fen);
	make_move(next, position, move);
	assert_int(position_piece_at(next, str_to_index("e2")), ==, nonpiece);
	assert_int(position_piece_at(next, str_to_index("e4")), ==, pawn);
	position_destroy(position);
	position_destroy(next);
}

static void
test_move_str(void)
{
	struct position pos[1];
	char str[1024];
	const char *end;
	struct move m;

	end = position_read_fen(positions[0].FEN, pos);
	assert_ptr_not_null(end);
	assert_false(position_has_en_passant_index(pos));
	assert_false(position_has_en_passant_target(pos));
	assert_int(position_turn(pos), ==, black);
	assert_ptr_equal(end, (positions[0].FEN + strlen(positions[0].FEN)));
	end = position_print_fen_no_move_count(str, pos);
	assert_string_equal(str, positions[0].FEN);
	assert_ptr_equal(end, (str + strlen(positions[0].FEN)));
	assert_int(read_move(pos, "h8h7", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(h8, h7, king, 0)));
	print_move(str, pos, m, mn_coordinate);
	assert_int(strcmp(str, "h8h7"), ==, 0);
	assert_int(read_move(pos, "h8g8", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(h8, g8, king, 0)));
	print_move(str, pos, m, mn_coordinate);
	assert_string_equal(str, "h8g8");
	assert_int(read_move(pos, "b5b4", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(b5, b4, pawn, 0)));
	print_move(str, pos, m, mn_coordinate);
	assert_string_equal(str, "b5b4");
	assert_int(read_move(pos, "b5a4", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(b5, a4, pawn, pawn)));
	print_move(str, pos, m, mn_coordinate);
	assert_string_equal(str, "b5a4");
	assert_int(read_move(pos, "b5c4", &m), !=, 0);
	assert_int(read_move(pos, "Kh7", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(h8, h7, king, 0)));
	print_move(str, pos, m, mn_san);
	assert_string_equal(str, "Kh7");
	assert_int(read_move(pos, "Kg8", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(h8, g8, king, 0)));
	print_move(str, pos, m, mn_san);
	assert_string_equal(str, "Kg8");
	assert_int(read_move(pos, "b4", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(b5, b4, pawn, 0)));
	print_move(str, pos, m, mn_san);
	assert_string_equal(str, "b4");
	assert_int(read_move(pos, "bxa4", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(b5, a4, pawn, pawn)));
	print_move(str, pos, m, mn_san);
	assert_string_equal(str, "bxa4");
	assert_int(read_move(pos, "bxc4", &m), !=, 0);
	assert_int(m.captured, ==, pawn);
	assert_int(read_move(pos, "o-o", &m), !=, 0);
	assert_int(read_move(pos, "o-o-o", &m), !=, 0);

	end = position_read_fen(positions[1].FEN, pos);
	assert_ptr_not_null(end);
	assert_false(position_has_en_passant_index(pos));
	assert_false(position_has_en_passant_target(pos));
	assert_int(position_turn(pos), ==, white);
	assert_ptr_equal(end, (positions[1].FEN + strlen(positions[1].FEN)));
	end = position_print_fen_no_move_count(str, pos);
	assert_string_equal(str, positions[1].FEN);
	assert_ptr_equal(end, (str + strlen(positions[1].FEN)));
	assert_int(read_move(pos, "o-o", &m), !=, 0);
	assert_int(read_move(pos, "o-o-o", &m), !=, 0);

	end = position_read_fen(positions[2].FEN, pos);
	assert_ptr_not_null(end);
	assert_true(position_has_en_passant_index(pos));
	assert_true(position_has_en_passant_target(pos));
	assert_int(position_get_en_passant_index(pos), ==, d5);
	assert_int(position_get_en_passant_target(pos), ==, d6);
	assert_int(position_turn(pos), ==, white);
	assert_ptr_equal(end, (positions[2].FEN + strlen(positions[2].FEN)));
	end = position_print_fen(str, pos);
	assert_string_equal(str, positions[2].FEN);
	assert_ptr_equal(end, (str + strlen(positions[2].FEN)));
	assert_int(read_move(pos, "o-o", &m), ==, 0);
	assert_true(move_eq(m, mcastle_white_king_side()));
	assert_int(read_move(pos, "O-O", &m), ==, 0);
	assert_true(move_eq(m, mcastle_white_king_side()));
	print_move(str, pos, m, mn_coordinate);
	assert_string_equal(str, "e1g1");
	print_move(str, pos, m, mn_san);
	assert_string_equal(str, "O-O");
	assert_int(read_move(pos, "o-o-o", &m), !=, 0);
	assert_int(read_move(pos, "e5d6", &m), ==, 0);
	assert_true(move_eq(m, create_move_ep(e5, d6)));
	print_move(str, pos, m, mn_coordinate);
	assert_string_equal(str, "e5d6");
	assert_int(read_move(pos, "exd6", &m), ==, 0);
	assert_true(move_eq(m, create_move_ep(e5, d6)));
	print_move(str, pos, m, mn_san);
	assert_string_equal(str, "exd6e.p.");

	end = position_read_fen(positions[3].FEN, pos);
	assert_ptr_not_null(end);
	assert_false(position_has_en_passant_index(pos));
	assert_false(position_has_en_passant_target(pos));
	assert_int(position_turn(pos), ==, white);
	assert_ptr_equal(end, (positions[3].FEN + strlen(positions[3].FEN)));
	end = position_print_fen_no_move_count(str, pos);
	assert_string_equal(str, positions[3].FEN);
	assert_ptr_equal(end, (str + strlen(positions[3].FEN)));
	assert_int(read_move(pos, "o-o", &m), ==, 0);
	assert_true(move_eq(m, mcastle_white_king_side()));
	assert_int(read_move(pos, "O-O", &m), ==, 0);
	assert_true(move_eq(m, mcastle_white_king_side()));
	print_move(str, pos, m, mn_coordinate);
	assert_string_equal(str, "e1g1");
	print_move(str, pos, m, mn_san);
	assert_string_equal(str, "O-O");
	assert_int(read_move(pos, "c1b2", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(c1, b2, bishop, 0)));
	assert_int(read_move(pos, "Bb2", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(c1, b2, bishop, 0)));
	assert_int(read_move(pos, "Bb2+", &m), ==, 0);
	assert_true(move_eq(m, create_move_g(c1, b2, bishop, 0)));
	print_move(str, pos, m, mn_san);
	assert_string_equal(str, "Bb2+");

	end = position_read_fen(positions[4].FEN, pos);
	assert_ptr_not_null(end);
	assert_false(position_has_en_passant_index(pos));
	assert_false(position_has_en_passant_target(pos));
	assert_int(position_turn(pos), ==, white);
	assert_ptr_equal(end, (positions[4].FEN + strlen(positions[4].FEN)));
	end = position_print_fen_no_move_count(str, pos);
	assert_string_equal(str, positions[4].FEN);
	assert_ptr_equal(end, (str + strlen(positions[4].FEN)));
	assert_int(read_move(pos, "c7b8q", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, b8, queen, knight)));
	assert_int(read_move(pos, "c7b8Q", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, b8, queen, knight)));
	assert_int(read_move(pos, "c7b8n", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, b8, knight, knight)));
	assert_int(read_move(pos, "c7b8r", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, b8, rook, knight)));
	assert_int(read_move(pos, "c7b8b", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, b8, bishop, knight)));
	assert_int(read_move(pos, "c7d8q", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, d8, queen, queen)));
	assert_int(read_move(pos, "c7d8Q", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, d8, queen, queen)));
	assert_int(read_move(pos, "cxd8=Q", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, d8, queen, queen)));
	assert_int(read_move(pos, "cxd8=Q+", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, d8, queen, queen)));
	assert_int(read_move(pos, "cxd8=R+", &m), ==, 0);
	assert(move_eq(m, create_move_pr(c7, d8, rook, queen)));

	end = position_read_fen(positions[5].FEN, pos);
	assert_ptr_not_null(end);
	assert_false(position_has_en_passant_index(pos));
	assert_true(position_has_en_passant_target(pos));
	assert_int(position_turn(pos), ==, black);
	assert_ptr_equal(end, (positions[5].FEN + strlen(positions[5].FEN)));
	assert_int(position_half_move_count(pos), ==, 0);
	assert_int(position_full_move_count(pos), ==, 1);
	end = position_print_fen(str, pos);
	assert_string_equal(str, positions[5].FEN);

	end = position_read_fen(
	    "rnbqkbnr/ppppp2p/5p2/6p1/8/4P3/PPPP1PPP/RNBQKBNR w KQkq -",
	    pos);
	assert_ptr_not_null(end);
	assert_int(read_move(pos, "d1h5", &m), ==, 0);
	assert(move_eq(m, create_move_g(d1, h5, queen, 0)));
	assert_int(read_move(pos, "Qh5", &m), ==, 0);
	assert(move_eq(m, create_move_g(d1, h5, queen, 0)));
	assert_int(read_move(pos, "Qh5+", &m), ==, 0);
	assert(move_eq(m, create_move_g(d1, h5, queen, 0)));
	assert_int(read_move(pos, "Qh5#", &m), ==, 0);
	assert(move_eq(m, create_move_g(d1, h5, queen, 0)));
	print_move(str, pos, m, mn_san);
	assert_string_equal(str, "Qh5#");
}

static void
test_board_print(void)
{
	char str[BOARD_BUFFER_LENGTH];
	struct position pos[1];

	position_read_fen(positions[0].FEN, pos);
	board_print(str, pos, false, false);
	assert_string_equal(positions[0].board, str);

	position_read_fen(positions[1].FEN, pos);
	board_print(str, pos, false, false);
	assert_string_equal(positions[1].board, str);

	position_read_fen(positions[2].FEN, pos);
	board_print(str, pos, false, false);
	assert_string_equal(positions[2].board, str);

	position_read_fen(positions[3].FEN, pos);
	board_print(str, pos, false, false);
	assert_string_equal(positions[3].board, str);

	position_read_fen(positions[4].FEN, pos);
	board_print(str, pos, false, false);
	assert_string_equal(positions[4].board, str);

	position_read_fen(positions[4].FEN, pos);
	board_print(str, pos, false, false);
	assert_string_equal(positions[4].board, str);
}

void
run_tests(void)
{
	test_chars();
	test_coordinates();
	test_invalid_fens();
	test_fen_basic();
	test_move_str();
	test_board_print();
}
