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

#include "tests.h"

#include "position.h"

void
taltos::test::run_tests(void)
{
	struct position pos;

	position_read_fen(&pos, start_position_fen, nullptr, nullptr);
	assert(pos.occupied == (bb_rank_1 | bb_rank_2 | bb_rank_7 | bb_rank_8));
	assert(pos.map[0] == (bb_rank_1 | bb_rank_2));
	assert(pos.map[1] == (bb_rank_7 | bb_rank_8));
	assert(pos.attack[pawn] == bb_rank_3);
	assert(pos.attack[rook] == bb(b1, a2, g1, h2));
	assert(pos.attack[bishop] == bb(b2, d2, e2, g2));
	assert(pos.attack[knight] == bb(a3, c3, d2, e2, f3, h3));
	assert(pos.attack[queen] == bb(c1, c2, d2, e2, e1));
	assert(pos.attack[king] == bb(d1, d2, e2, f2, f1));
	assert(pos.attack[0] == (bb(b1, c1, d1, e1, f1, g1) | bb_rank_2 | bb_rank_3));
	assert(pos.attack[opponent_pawn] == bb_rank_6);
	assert(pos.attack[opponent_rook] == bb(b8, a7, g8, h7));
	assert(pos.attack[opponent_bishop] == bb(b7, d7, e7, g7));
	assert(pos.attack[opponent_knight] == bb(a6, c6, d7, e7, f6, h6));
	assert(pos.attack[opponent_queen] == bb(c8, c7, d7, e7, e8));
	assert(pos.attack[opponent_king] == bb(d8, d7, e7, f7, f8));
	assert(pos.attack[1] == (bb(b8, c8, d8, e8, f8, g8) | bb_rank_7 | bb_rank_6));
	assert(is_empty(pos.king_attack_map));

	position_read_fen(&pos,
	    "rnbqkbnr/pppp1ppp/4p3/1B6/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 1 2",
	    nullptr, nullptr);
	assert(pos.occupied ==
	    ((bb_rank_1 | bb_rank_2 | bb_rank_7 | bb_rank_8 | bb(e4, e6, b5)) & ~bb(f1, e2, e7)).flipped());
	assert(pos.map[0] == ((bb_rank_7 | bb_rank_8 | bb(e6)) & ~bb(e7)).flipped());
	assert(pos.map[1] == ((bb_rank_1 | bb_rank_2 | bb(e4, b5)) & ~bb(f1, e2)).flipped());
	assert(pos.attack[opponent_pawn] == (bb_rank_3 | bb(d5, f5)).flipped());
	assert(pos.attack[opponent_rook] == bb(b1, a2, g1, h2).flipped());
	assert(pos.attack[opponent_bishop] == bb(b2, d2, a6, a4, c6, c4, d7, d3, e2, f1).flipped());
	assert(pos.attack[opponent_knight] == bb(a3, c3, d2, e2, f3, h3).flipped());
	assert(pos.attack[opponent_queen] == bb(c1, c2, d2, e2, e1, f3, g4, h5).flipped());
	assert(pos.attack[opponent_king] == bb(d1, d2, e2, f2, f1).flipped());
	assert(pos.attack[pawn] == (bb_rank_6 | bb(d5, f5)).flipped());
	assert(pos.attack[rook] == bb(b8, a7, g8, h7).flipped());
	assert(pos.attack[bishop] == bb(b7, d7, e7, g7, d6, c5, b4, a3).flipped());
	assert(pos.attack[knight] == bb(a6, c6, d7, e7, f6, h6).flipped());
	assert(pos.attack[queen] == bb(c8, c7, d7, e7, e8, f6, g5, h4).flipped());
	assert(pos.attack[king] == bb(d8, d7, e7, f7, f8).flipped());
	assert(is_empty(pos.king_attack_map));

	position_read_fen(&pos,
	    "rnbqkbnr/ppppp1pp/5p2/7Q/8/4P3/PPPP1PPP/RNB1KBNR b KQkq - 1 2",
	    nullptr, nullptr);
	assert(pos.occupied ==
	    ((bb_rank_1 | bb_rank_2 | bb_rank_7 | bb_rank_8 | bb(e3, h5, f6))
	     & ~bb(e2, d1, f7)).flipped());
	assert(pos.map[0] == ((bb_rank_7 | bb_rank_8 | bb(f6)) & ~bb(f7)).flipped());
	assert(pos.map[1] == ((bb_rank_1 | bb_rank_2 | bb(e3, h5)) & ~bb(e2, d1)).flipped());
	assert(pos.attack[opponent_pawn] == (bb_rank_3 | bb(d4, f4)).flipped());
	assert(pos.attack[opponent_rook] == bb(b1, a2, g1, h2).flipped());
	assert(pos.attack[opponent_bishop] == bb(b2, d2, a6, b5, c4, d3, e2, g2).flipped());
	assert(pos.attack[opponent_knight] == bb(a3, c3, d2, e2, f3, h3).flipped());
	assert(pos.attack[opponent_queen] ==
	    ((bb(d1, e2, f3, g4) | bb_rank_5 | bb(g6, f7, e8) | bb_file_h) & ~bb(h8, h1, h5)).flipped());
	assert(pos.attack[opponent_king] == bb(d1, d2, e2, f2, f1).flipped());
	assert(pos.attack[pawn] == (bb_rank_6 | bb(e5, g5)).flipped());
	assert(pos.attack[rook] == bb(b8, a7, g8, h7).flipped());
	assert(pos.attack[bishop] == bb(b7, d7, e7, g7).flipped());
	assert(pos.attack[knight] == bb(a6, c6, d7, e7, f6, h6).flipped());
	assert(pos.attack[queen] == bb(c8, c7, d7, e7, e8).flipped());
	assert(pos.attack[king] == bb(d8, d7, e7, f7, f8).flipped());
	assert(pos.king_attack_map == bb(f7, g6, h5).flipped());

	position_read_fen(&pos,
	    "rnb1kbnr/pppp1ppp/4p3/8/7q/5PP1/PPPPP2P/RNBQKBNR w KQkq - 4 3",
	    nullptr, nullptr);
	assert(pos.attack[pawn] == ((bb_rank_3 | bb(e4, f4, g4, h4)) & ~bb(h3)));
	assert(is_empty(pos.king_attack_map));

	position_read_fen(&pos,
	    "rnb1kbnr/pp1ppppp/8/q1p5/1P6/3P4/P1P1PPPP/RNBQKBNR w KQkq - 1 3",
	    nullptr, nullptr);
	assert(is_empty(pos.king_attack_map));
}
