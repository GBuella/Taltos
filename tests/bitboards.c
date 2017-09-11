
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "tests.h"

#include "position.h"

void
run_tests(void)
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

	position_read_fen(&pos,
	    "rnb1kbnr/pppp1ppp/4p3/8/7q/5PP1/PPPPP2P/RNBQKBNR w KQkq - 4 3",
	    NULL, NULL);
	assert(pos.attack[pawn] ==
	    ((RANK_3 | SQ_E4 | SQ_F4| SQ_G4 | SQ_H4) & ~(SQ_H3)));
	assert(is_empty(pos.king_attack_map));

	position_read_fen(&pos,
	    "rnb1kbnr/pp1ppppp/8/q1p5/1P6/3P4/P1P1PPPP/RNBQKBNR w KQkq - 1 3",
	    NULL, NULL);
	assert(is_empty(pos.king_attack_map));
}
