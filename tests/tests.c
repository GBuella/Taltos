
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "str_util.h"
#include "perft.h"
#include "search.h"
#include "eval.h"
#include "game.h"
#include "position.h"
#include "hash.h"
#include "tests.h"

static void
char_test(void)
{
	assert(char_to_file('a') == file_a);
	assert(char_to_file('b') == file_b);
	assert(char_to_file('h') == file_h);
	assert(char_to_file('A') == file_a);
	assert(char_to_file('B') == file_b);
	assert(char_to_file('H') == file_h);

	assert(char_to_rank('1', white) == rank_1);
	assert(char_to_rank('2', white) == rank_2);
	assert(char_to_rank('3', white) == rank_3);
	assert(char_to_rank('4', white) == rank_4);
	assert(char_to_rank('5', white) == rank_5);
	assert(char_to_rank('8', white) == rank_8);
	assert(char_to_rank('1', black) == rank_8);
	assert(char_to_rank('2', black) == rank_7);
	assert(char_to_rank('3', black) == rank_6);
	assert(char_to_rank('4', black) == rank_5);
	assert(char_to_rank('5', black) == rank_4);
	assert(char_to_rank('8', black) == rank_1);

	assert(index_to_file_ch(0) == 'h');
	assert(index_to_file_ch(1) == 'g');
	assert(index_to_file_ch(7) == 'a');
	assert(index_to_file_ch(63) == 'a');

	assert(index_to_rank_ch(0, white) == '8');
	assert(index_to_rank_ch(1, white) == '8');
	assert(index_to_rank_ch(8 + 7, white) == '7');
	assert(index_to_rank_ch(63, white) == '1');
	assert(index_to_rank_ch(0, black) == '1');
	assert(index_to_rank_ch(1, black) == '1');
	assert(index_to_rank_ch(8 + 7, black) == '2');
	assert(index_to_rank_ch(63, black) == '8');

	assert(piece_to_char(queen) == 'q');

	assert(square_to_char(queen, white) == 'Q');
	assert(square_to_char(queen, black) == 'q');

	assert(is_file('f'));
	assert(is_file('F'));
	assert(!is_file('4'));
	assert(!is_file('i'));
	assert(!is_file(' '));

	assert(is_rank('1'));
	assert(is_rank('6'));
	assert(!is_rank('9'));
	assert(!is_rank('0'));
	assert(!is_rank('a'));
	assert(!is_rank(' '));

}

static void
position_move_test(void)
{
	struct position *position;
	struct position *next;
	move move;
	char str[FEN_BUFFER_LENGTH + MOVE_STR_BUFFER_LENGTH];
	static const char *empty_fen = "8/8/8/8/8/8/8/8 w - - 0 1";
	unsigned hm, fm;
	int ep_index;
	enum player turn;

	position = position_allocate();
	next = position_allocate();
	assert(position_print_fen_full(position, str, 0, 1, 0, white)
	    == str + strlen(empty_fen));
	assert(strcmp(empty_fen, str) == 0);
	assert(NULL != position_read_fen_full(position, start_position_fen,
	    &ep_index, &fm, &hm, &turn));
	assert(ep_index == 0);
	assert(hm == 0 && fm == 1 && turn == white);
	assert(position_print_fen_full(position, str, 0, 1, 0, white)
	    == str + strlen(start_position_fen));
	assert(strcmp(str, start_position_fen) == 0);
	move = create_move_t(str_to_index("e2", white),
	    str_to_index("e4", white),
	    mt_pawn_double_push, pawn, 0);
	setup_registers();
	make_move(next, position, move);
	assert(position_piece_at(next, str_to_index("e2", black)) == nonpiece);
	assert(position_piece_at(next, str_to_index("e4", black)) == pawn);
	position_destroy(position);
	position_destroy(next);
}

static void
position_bitboards_test(void)
{
	struct position pos;

	position_read_fen(&pos, start_position_fen, NULL, NULL);
	assert(pos.occupied == (RANK_1 | RANK_2 | RANK_7 | RANK_8));
	assert(pos.map[0] == (RANK_1 | RANK_2));
	assert(pos.map[1] == (RANK_7 | RANK_8));
	assert(pos.attack[pawn] == RANK_3);
	assert(pos.attack[rook] ==
	    (SQ_B1 | SQ_A2 | SQ_G1 | SQ_H2));
	assert(pos.attack[bishop] ==
	    (SQ_B2 | SQ_D2 | SQ_E2 | SQ_G2));
	assert(pos.attack[knight] ==
	    (SQ_A3 | SQ_C3 | SQ_D2 | SQ_E2 | SQ_F3 | SQ_H3));
	assert(pos.attack[queen] ==
	    (SQ_C1 | SQ_C2 | SQ_D2 | SQ_E2 | SQ_E1));
	assert(pos.attack[king] ==
	    (SQ_D1 | SQ_D2 | SQ_E2 | SQ_F2 | SQ_F1));
	assert(pos.attack[0] ==
	    (SQ_B1 | SQ_C1 | SQ_D1 | SQ_E1 | SQ_F1 | SQ_G1 | RANK_2 | RANK_3));
	assert(pos.attack[opponent_pawn] == RANK_6);
	assert(pos.attack[opponent_rook] ==
	    (SQ_B8 | SQ_A7 | SQ_G8 | SQ_H7));
	assert(pos.attack[opponent_bishop] ==
	    (SQ_B7 | SQ_D7 | SQ_E7 | SQ_G7));
	assert(pos.attack[opponent_knight] ==
	    (SQ_A6 | SQ_C6 | SQ_D7 | SQ_E7 | SQ_F6 | SQ_H6));
	assert(pos.attack[opponent_queen] ==
	    (SQ_C8 | SQ_C7 | SQ_D7 | SQ_E7 | SQ_E8));
	assert(pos.attack[opponent_king] ==
	    (SQ_D8 | SQ_D7 | SQ_E7 | SQ_F7 | SQ_F8));
	assert(pos.attack[1] ==
	    (SQ_B8 | SQ_C8 | SQ_D8 | SQ_E8 | SQ_F8 | SQ_G8 | RANK_7 | RANK_6));
	assert(is_empty(pos.king_attack_map));
	assert(is_empty(pos.bpin_map));
	assert(is_empty(pos.rpin_map));

	position_read_fen(&pos,
	    "rnbqkbnr/pppp1ppp/4p3/1B6/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 1 2",
	    NULL, NULL);
	assert(pos.occupied ==
	    bswap((RANK_1 | RANK_2 | RANK_7 | RANK_8 | SQ_E4 | SQ_E6 | SQ_B5)
	    & ~(SQ_F1 | SQ_E2 | SQ_E7)));
	assert(pos.map[0] ==
	    bswap((RANK_7 | RANK_8 | SQ_E6) & ~SQ_E7));
	assert(pos.map[1] ==
	    bswap((RANK_1 | RANK_2 | SQ_E4 | SQ_B5) & ~(SQ_F1 | SQ_E2)));
	assert(pos.attack[opponent_pawn] ==
	    bswap(RANK_3 | SQ_D5 | SQ_F5));
	assert(pos.attack[opponent_rook] ==
	    bswap(SQ_B1 | SQ_A2 | SQ_G1 | SQ_H2));
	assert(pos.attack[opponent_bishop] ==
	    bswap(SQ_B2 | SQ_D2 | SQ_A6 | SQ_A4 | SQ_C6 |
	    SQ_C4 | SQ_D7 | SQ_D3 | SQ_E2 | SQ_F1));
	assert(pos.attack[opponent_knight] ==
	    bswap(SQ_A3 | SQ_C3 | SQ_D2 | SQ_E2 | SQ_F3 | SQ_H3));
	assert(pos.attack[opponent_queen] ==
	    bswap(SQ_C1 | SQ_C2 | SQ_D2 | SQ_E2 |
	    SQ_E1 | SQ_F3 | SQ_G4 | SQ_H5));
	assert(pos.attack[opponent_king] ==
	    bswap(SQ_D1 | SQ_D2 | SQ_E2 | SQ_F2 | SQ_F1));
	assert(pos.attack[pawn] ==
	    bswap(RANK_6 | SQ_D5 | SQ_F5));
	assert(pos.attack[rook] ==
	    bswap(SQ_B8 | SQ_A7 | SQ_G8 | SQ_H7));
	assert(pos.attack[bishop] ==
	    bswap(SQ_B7 | SQ_D7 | SQ_E7 | SQ_G7 |
	    SQ_D6 | SQ_C5 | SQ_B4 | SQ_A3));
	assert(pos.attack[knight] ==
	    bswap(SQ_A6 | SQ_C6 | SQ_D7 | SQ_E7 | SQ_F6 | SQ_H6));
	assert(pos.attack[queen] ==
	    bswap(SQ_C8 | SQ_C7 | SQ_D7 | SQ_E7 |
	    SQ_E8 | SQ_F6 | SQ_G5 | SQ_H4));
	assert(pos.attack[king] ==
	    bswap(SQ_D8 | SQ_D7 | SQ_E7 | SQ_F7 | SQ_F8));
	assert(is_empty(pos.king_attack_map));
	assert(pos.bpin_map == bswap(SQ_D7 | SQ_C6 | SQ_B5));
	assert(is_empty(pos.rpin_map));

	position_read_fen(&pos,
	    "rnbqkbnr/ppppp1pp/5p2/7Q/8/4P3/PPPP1PPP/RNB1KBNR b KQkq - 1 2",
	    NULL, NULL);
	assert(pos.occupied ==
	    bswap((RANK_1 | RANK_2 | RANK_7 | RANK_8 | SQ_E3 | SQ_H5 | SQ_F6)
	    & ~(SQ_E2 | SQ_D1 | SQ_F7)));
	assert(pos.map[0] ==
	    bswap((RANK_7 | RANK_8 | SQ_F6) & ~SQ_F7));
	assert(pos.map[1] ==
	    bswap((RANK_1 | RANK_2 | SQ_E3 | SQ_H5) & ~(SQ_E2 | SQ_D1)));
	assert(pos.attack[opponent_pawn] ==
	    bswap(RANK_3 | SQ_D4 | SQ_F4));
	assert(pos.attack[opponent_rook] ==
	    bswap(SQ_B1 | SQ_A2 | SQ_G1 | SQ_H2));
	assert(pos.attack[opponent_bishop] ==
	    bswap(SQ_B2 | SQ_D2 | SQ_A6 | SQ_B5 |
	    SQ_C4 | SQ_D3 | SQ_E2 | SQ_G2));
	assert(pos.attack[opponent_knight] ==
	    bswap(SQ_A3 | SQ_C3 | SQ_D2 | SQ_E2 | SQ_F3 | SQ_H3));
	assert(pos.attack[opponent_queen] ==
	    bswap((SQ_D1 | SQ_E2 | SQ_F3 | SQ_G4 | RANK_5 |
	    SQ_G6 | SQ_F7 | SQ_E8 | FILE_H) & ~(SQ_H8 | SQ_H1 | SQ_H5)));
	assert(pos.attack[opponent_king] ==
	    bswap(SQ_D1 | SQ_D2 | SQ_E2 | SQ_F2 | SQ_F1));
	assert(pos.attack[pawn] ==
	    bswap(RANK_6 | SQ_E5 | SQ_G5));
	assert(pos.attack[rook] ==
	    bswap(SQ_B8 | SQ_A7 | SQ_G8 | SQ_H7));
	assert(pos.attack[bishop] ==
	    bswap(SQ_B7 | SQ_D7 | SQ_E7 | SQ_G7));
	assert(pos.attack[knight] ==
	    bswap(SQ_A6 | SQ_C6 | SQ_D7 | SQ_E7 | SQ_F6 | SQ_H6));
	assert(pos.attack[queen] ==
	    bswap(SQ_C8 | SQ_C7 | SQ_D7 | SQ_E7 | SQ_E8));
	assert(pos.attack[king] ==
	    bswap(SQ_D8 | SQ_D7 | SQ_E7 | SQ_F7 | SQ_F8));
	assert(pos.king_attack_map ==
	    bswap(SQ_F7 | SQ_G6 | SQ_H5));
	assert(is_empty(pos.bpin_map));
	assert(is_empty(pos.rpin_map));

	position_read_fen(&pos,
	    "rnb1kbnr/pppp1ppp/4p3/8/7q/5PP1/PPPPP2P/RNBQKBNR w KQkq - 4 3",
	    NULL, NULL);
	assert(pos.attack[pawn] ==
	    ((RANK_3 | SQ_E4 | SQ_F4| SQ_G4 | SQ_H4) & ~(SQ_H3)));
	assert(is_empty(pos.king_attack_map));
	assert(pos.bpin_map == (SQ_F2 | SQ_G3 | SQ_H4));
	assert(is_empty(pos.rpin_map));

	position_read_fen(&pos,
	    "rnb1kbnr/pp1ppppp/8/q1p5/1P6/3P4/P1P1PPPP/RNBQKBNR w KQkq - 1 3",
	    NULL, NULL);
	assert(is_empty(pos.king_attack_map));
	assert(pos.bpin_map == (SQ_A5 | SQ_B4 | SQ_C3 | SQ_D2));
	assert(is_empty(pos.rpin_map));
}

static void
game_test(void)
{
	struct game *game, *other;
	move move;

	game = game_create();
	if (game == NULL) abort();
	assert(game_turn(game) == white);
	assert(game_history_revert(game) != 0);
	assert(game_history_forward(game) != 0);
	assert(game_full_move_count(game) == 1);
	assert(game_half_move_count(game) == 0);
	move = create_move_t(ind(rank_2, file_e), ind(rank_4, file_e),
	    mt_pawn_double_push, pawn, 0);
	assert(game_append(game, move) == 0);
	other = game_copy(game);
	if (other == NULL) abort();
	assert(game_turn(game) == black);
	assert(game_turn(other) == black);
	assert(game_history_revert(other) == 0);
	game_destroy(other);
	move = create_move_t(str_to_index("e7", black),
	    str_to_index("e5", black),
	    mt_pawn_double_push, pawn, 0);
	assert(game_append(game, move) == 0);
	assert(game_turn(game) == white);
	game_destroy(game);
}

void
pkey_test()
{
	static const struct {
		const char *FEN;
		const uint64_t polyglot_key;
	} data[] = {
	{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
		UINT64_C(0x463b96181691fc9c)},
	{"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
		UINT64_C(0x823c9b50fd114196)},
	{"rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",
		UINT64_C(0x0756b94461c50fb0)},
	{"rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2",
		UINT64_C(0x662fafb965db29d4)},
	{"rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
		UINT64_C(0x22a48b5a8e47ff78)},
	{"rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR b kq - 0 3",
		UINT64_C(0x652a607ca3f242c1)},
	{"rnbq1bnr/ppp1pkpp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR w - - 0 4",
		UINT64_C(0x00fdd303c946bdd9)},
	{"rnbqkbnr/p1pppppp/8/8/PpP4P/8/1P1PPPP1/RNBQKBNR b KQkq c3 0 3",
		UINT64_C(0x3c8123ea7b067637)},
	{"rnbqkbnr/p1pppppp/8/8/P6P/R1p5/1P1PPPP1/1NBQKBNR b Kkq - 0 4",
		UINT64_C(0x5c3f9b829b279560)}
	};
	struct position position[1];

	for (size_t i = 0; i < ARRAY_LENGTH(data); ++i) {
		enum player turn;

		position_read_fen(position, data[i].FEN, NULL, &turn);
		assert(position_polyglot_key(position, turn)
		    == data[i].polyglot_key);
	}
}

void
run_internal_tests(void)
{
	char_test();
	run_string_tests();
	pkey_test();
	position_move_test();
	position_bitboards_test();
	game_test();
	run_hash_table_tests();
}
