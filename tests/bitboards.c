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

#include "position.h"

void
run_tests(void)
{
	struct position pos;

	position_read_fen(start_position_fen, &pos);
	assert_int64(pos.occupied, ==, (RANK_1 | RANK_2 | RANK_7 | RANK_8));
	assert_int64(pos.map[0], ==, (RANK_1 | RANK_2));
	assert_int64(pos.map[1], ==, (RANK_7 | RANK_8));
	assert_int64(pos.attack[white_pawn], ==, RANK_3);
	assert_int64(pos.attack[white_rook], ==,
	    (SQ_B1 | SQ_A2 | SQ_G1 | SQ_H2));
	assert_int64(pos.attack[white_bishop], ==,
	    (SQ_B2 | SQ_D2 | SQ_E2 | SQ_G2));
	assert_int64(pos.attack[white_knight], ==,
	    (SQ_A3 | SQ_C3 | SQ_D2 | SQ_E2 | SQ_F3 | SQ_H3));
	assert_int64(pos.attack[white_queen], ==,
	    (SQ_C1 | SQ_C2 | SQ_D2 | SQ_E2 | SQ_E1));
	assert_int64(pos.attack[white_king], ==,
	    (SQ_D1 | SQ_D2 | SQ_E2 | SQ_F2 | SQ_F1));
	assert_int64(pos.attack[white], ==,
	    (SQ_B1 | SQ_C1 | SQ_D1 | SQ_E1 | SQ_F1 | SQ_G1 | RANK_2 | RANK_3));
	assert_int64(pos.attack[black_pawn], ==, RANK_6);
	assert_int64(pos.attack[black_rook], ==,
	    (SQ_B8 | SQ_A7 | SQ_G8 | SQ_H7));
	assert_int64(pos.attack[black_bishop], ==,
	    (SQ_B7 | SQ_D7 | SQ_E7 | SQ_G7));
	assert_int64(pos.attack[black_knight], ==,
	    (SQ_A6 | SQ_C6 | SQ_D7 | SQ_E7 | SQ_F6 | SQ_H6));
	assert_int64(pos.attack[black_queen], ==,
	    (SQ_C8 | SQ_C7 | SQ_D7 | SQ_E7 | SQ_E8));
	assert_int64(pos.attack[black_king], ==,
	    (SQ_D8 | SQ_D7 | SQ_E7 | SQ_F7 | SQ_F8));
	assert_int64(pos.attack[black], ==,
	    (SQ_B8 | SQ_C8 | SQ_D8 | SQ_E8 | SQ_F8 | SQ_G8 | RANK_7 | RANK_6));
	assert_true(is_empty(pos.king_attack_map));

	position_read_fen(
	    "rnbqkbnr/pppp1ppp/4p3/1B6/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 1 2",
	    &pos);
	assert_int64(pos.occupied, ==,
	    ((RANK_1 | RANK_2 | RANK_7 | RANK_8 | SQ_E4 | SQ_E6 | SQ_B5)
	    & ~(SQ_F1 | SQ_E2 | SQ_E7)));
	assert_int64(pos.map[black], ==,
	    ((RANK_7 | RANK_8 | SQ_E6) & ~SQ_E7));
	assert_int64(pos.map[white], ==,
	    ((RANK_1 | RANK_2 | SQ_E4 | SQ_B5) & ~(SQ_F1 | SQ_E2)));
	assert_int64(pos.attack[white_pawn], ==,
	    (RANK_3 | SQ_D5 | SQ_F5));
	assert_int64(pos.attack[white_rook], ==,
	    (SQ_B1 | SQ_A2 | SQ_G1 | SQ_H2));
	assert_int64(pos.attack[white_bishop], ==,
	    (SQ_B2 | SQ_D2 | SQ_A6 | SQ_A4 | SQ_C6 |
	    SQ_C4 | SQ_D7 | SQ_D3 | SQ_E2 | SQ_F1));
	assert_int64(pos.attack[white_knight], ==,
	    (SQ_A3 | SQ_C3 | SQ_D2 | SQ_E2 | SQ_F3 | SQ_H3));
	assert_int64(pos.attack[white_queen], ==,
	    (SQ_C1 | SQ_C2 | SQ_D2 | SQ_E2 |
	    SQ_E1 | SQ_F3 | SQ_G4 | SQ_H5));
	assert_int64(pos.attack[white_king], ==,
	    (SQ_D1 | SQ_D2 | SQ_E2 | SQ_F2 | SQ_F1));
	assert_int64(pos.attack[black_pawn], ==,
	    (RANK_6 | SQ_D5 | SQ_F5));
	assert_int64(pos.attack[black_rook], ==,
	    (SQ_B8 | SQ_A7 | SQ_G8 | SQ_H7));
	assert_int64(pos.attack[black_bishop], ==,
	    (SQ_B7 | SQ_D7 | SQ_E7 | SQ_G7 |
	    SQ_D6 | SQ_C5 | SQ_B4 | SQ_A3));
	assert_int64(pos.attack[black_knight], ==,
	    (SQ_A6 | SQ_C6 | SQ_D7 | SQ_E7 | SQ_F6 | SQ_H6));
	assert_int64(pos.attack[black_queen], ==,
	    (SQ_C8 | SQ_C7 | SQ_D7 | SQ_E7 |
	    SQ_E8 | SQ_F6 | SQ_G5 | SQ_H4));
	assert_int64(pos.attack[black_king], ==,
	    (SQ_D8 | SQ_D7 | SQ_E7 | SQ_F7 | SQ_F8));
	assert_true(is_empty(pos.king_attack_map));

	position_read_fen(
	    "rnbqkbnr/ppppp1pp/5p2/7Q/8/4P3/PPPP1PPP/RNB1KBNR b KQkq - 1 2",
	    &pos);
	assert_int64(pos.occupied, ==,
	    ((RANK_1 | RANK_2 | RANK_7 | RANK_8 | SQ_E3 | SQ_H5 | SQ_F6)
	    & ~(SQ_E2 | SQ_D1 | SQ_F7)));
	assert_int64(pos.map[black], ==,
	    ((RANK_7 | RANK_8 | SQ_F6) & ~SQ_F7));
	assert_int64(pos.map[white], ==,
	    ((RANK_1 | RANK_2 | SQ_E3 | SQ_H5) & ~(SQ_E2 | SQ_D1)));
	assert_int64(pos.attack[white_pawn], ==,
	    (RANK_3 | SQ_D4 | SQ_F4));
	assert_int64(pos.attack[white_rook], ==,
	    (SQ_B1 | SQ_A2 | SQ_G1 | SQ_H2));
	assert_int64(pos.attack[white_bishop], ==,
	    (SQ_B2 | SQ_D2 | SQ_A6 | SQ_B5 | SQ_C4 | SQ_D3 | SQ_E2 | SQ_G2));
	assert_int64(pos.attack[white_knight], ==,
	    (SQ_A3 | SQ_C3 | SQ_D2 | SQ_E2 | SQ_F3 | SQ_H3));
	assert_int64(pos.attack[white_queen], ==,
	    ((SQ_D1 | SQ_E2 | SQ_F3 | SQ_G4 | RANK_5 |
	    SQ_G6 | SQ_F7 | SQ_E8 | FILE_H) & ~(SQ_H8 | SQ_H1 | SQ_H5)));
	assert_int64(pos.attack[white_king], ==,
	    (SQ_D1 | SQ_D2 | SQ_E2 | SQ_F2 | SQ_F1));
	assert_int64(pos.attack[black_pawn], ==,
	    (RANK_6 | SQ_E5 | SQ_G5));
	assert_int64(pos.attack[black_rook], ==,
	    (SQ_B8 | SQ_A7 | SQ_G8 | SQ_H7));
	assert_int64(pos.attack[black_bishop], ==,
	    (SQ_B7 | SQ_D7 | SQ_E7 | SQ_G7));
	assert_int64(pos.attack[black_knight], ==,
	    (SQ_A6 | SQ_C6 | SQ_D7 | SQ_E7 | SQ_F6 | SQ_H6));
	assert_int64(pos.attack[black_queen], ==,
	    (SQ_C8 | SQ_C7 | SQ_D7 | SQ_E7 | SQ_E8));
	assert_int64(pos.attack[black_king], ==,
	    (SQ_D8 | SQ_D7 | SQ_E7 | SQ_F7 | SQ_F8));
	assert_int64(pos.king_attack_map, ==,
	    (SQ_F7 | SQ_G6 | SQ_H5));

	position_read_fen(
	    "rnb1kbnr/pppp1ppp/4p3/8/7q/5PP1/PPPPP2P/RNBQKBNR w KQkq - 4 3",
	    &pos);
	assert_int64(pos.attack[white_pawn], ==,
	    ((RANK_3 | SQ_E4 | SQ_F4| SQ_G4 | SQ_H4) & ~(SQ_H3)));
	assert_true(is_empty(pos.king_attack_map));

	position_read_fen(
	    "rnb1kbnr/pp1ppppp/8/q1p5/1P6/3P4/P1P1PPPP/RNBQKBNR w KQkq - 1 3",
	    &pos);
	assert_true(is_empty(pos.king_attack_map));
}
