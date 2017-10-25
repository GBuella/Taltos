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

#ifndef TALTOS_CHESS_H
#define TALTOS_CHESS_H

namespace taltos
{

constexpr int opponent_of(int x)
{
	return x ^ 1;
}

constexpr int8_t nonpiece = 0;
constexpr int8_t pawn =	 2;
constexpr int8_t rook =	 4;
constexpr int8_t king =	 6;
constexpr int8_t bishop = 8;
constexpr int8_t knight = 10;
constexpr int8_t queen = 12;

constexpr int8_t opponent_pawn = opponent_of(pawn);
constexpr int8_t opponent_rook = opponent_of(rook);
constexpr int8_t opponent_king = opponent_of(king);
constexpr int8_t opponent_bishop = opponent_of(bishop);
constexpr int8_t opponent_knight = opponent_of(knight);
constexpr int8_t opponent_queen = opponent_of(queen);

static constexpr bool is_valid_piece(int piece)
{
	return (piece == pawn)
	    or (piece == king)
	    or (piece == rook)
	    or (piece == knight)
	    or (piece == bishop)
	    or (piece == queen);
}

constexpr int rank_8 = 0;
constexpr int rank_7 = 1;
constexpr int rank_6 = 2;
constexpr int rank_5 = 3;
constexpr int rank_4 = 4;
constexpr int rank_3 = 5;
constexpr int rank_2 = 6;
constexpr int rank_1 = 7;

constexpr int file_h = 0;
constexpr int file_g = 1;
constexpr int file_f = 2;
constexpr int file_e = 3;
constexpr int file_d = 4;
constexpr int file_c = 5;
constexpr int file_b = 6;
constexpr int file_a = 7;

constexpr int rsouth = +1;
constexpr int rnorth = -1;

constexpr int feast = -1;
constexpr int fwest = +1;

constexpr int north = -8;
constexpr int south = 8;
constexpr int west = 1;
constexpr int east = -1;


constexpr bool is_valid_file(int file)
{
	return (file & ~7) == 0;
}

constexpr bool is_valid_rank(int rank)
{
	return (rank & ~7) == 0;
}

constexpr bool is_valid_index(int index)
{
	return index >= 0 and index < 64;
}

static inline int ind_rank(int index)
{
	invariant(is_valid_index(index));
	return index / 8;
}

static inline int ind_file(int index)
{
	invariant(is_valid_index(index));
	return index & 7;
}

static inline int ind(int rank, int file)
{
	invariant(is_valid_rank(rank));
	invariant(is_valid_file(file));

	return (rank << 3) + file;
}

constexpr int ce_ind(int rank, int file)
{
	return (rank << 3) + file;
}


static inline int flip_i(int index)
{
	invariant(is_valid_index(index));
	return index ^ 0x38;
}

constexpr int a1 = ce_ind(rank_1, file_a);
constexpr int a2 = ce_ind(rank_2, file_a);
constexpr int a3 = ce_ind(rank_3, file_a);
constexpr int a4 = ce_ind(rank_4, file_a);
constexpr int a5 = ce_ind(rank_5, file_a);
constexpr int a6 = ce_ind(rank_6, file_a);
constexpr int a7 = ce_ind(rank_7, file_a);
constexpr int a8 = ce_ind(rank_8, file_a);

constexpr int b1 = ce_ind(rank_1, file_b);
constexpr int b2 = ce_ind(rank_2, file_b);
constexpr int b3 = ce_ind(rank_3, file_b);
constexpr int b4 = ce_ind(rank_4, file_b);
constexpr int b5 = ce_ind(rank_5, file_b);
constexpr int b6 = ce_ind(rank_6, file_b);
constexpr int b7 = ce_ind(rank_7, file_b);
constexpr int b8 = ce_ind(rank_8, file_b);

constexpr int c1 = ce_ind(rank_1, file_c);
constexpr int c2 = ce_ind(rank_2, file_c);
constexpr int c3 = ce_ind(rank_3, file_c);
constexpr int c4 = ce_ind(rank_4, file_c);
constexpr int c5 = ce_ind(rank_5, file_c);
constexpr int c6 = ce_ind(rank_6, file_c);
constexpr int c7 = ce_ind(rank_7, file_c);
constexpr int c8 = ce_ind(rank_8, file_c);

constexpr int d1 = ce_ind(rank_1, file_d);
constexpr int d2 = ce_ind(rank_2, file_d);
constexpr int d3 = ce_ind(rank_3, file_d);
constexpr int d4 = ce_ind(rank_4, file_d);
constexpr int d5 = ce_ind(rank_5, file_d);
constexpr int d6 = ce_ind(rank_6, file_d);
constexpr int d7 = ce_ind(rank_7, file_d);
constexpr int d8 = ce_ind(rank_8, file_d);

constexpr int e1 = ce_ind(rank_1, file_e);
constexpr int e2 = ce_ind(rank_2, file_e);
constexpr int e3 = ce_ind(rank_3, file_e);
constexpr int e4 = ce_ind(rank_4, file_e);
constexpr int e5 = ce_ind(rank_5, file_e);
constexpr int e6 = ce_ind(rank_6, file_e);
constexpr int e7 = ce_ind(rank_7, file_e);
constexpr int e8 = ce_ind(rank_8, file_e);

constexpr int f1 = ce_ind(rank_1, file_f);
constexpr int f2 = ce_ind(rank_2, file_f);
constexpr int f3 = ce_ind(rank_3, file_f);
constexpr int f4 = ce_ind(rank_4, file_f);
constexpr int f5 = ce_ind(rank_5, file_f);
constexpr int f6 = ce_ind(rank_6, file_f);
constexpr int f7 = ce_ind(rank_7, file_f);
constexpr int f8 = ce_ind(rank_8, file_f);

constexpr int g1 = ce_ind(rank_1, file_g);
constexpr int g2 = ce_ind(rank_2, file_g);
constexpr int g3 = ce_ind(rank_3, file_g);
constexpr int g4 = ce_ind(rank_4, file_g);
constexpr int g5 = ce_ind(rank_5, file_g);
constexpr int g6 = ce_ind(rank_6, file_g);
constexpr int g7 = ce_ind(rank_7, file_g);
constexpr int g8 = ce_ind(rank_8, file_g);

constexpr int h1 = ce_ind(rank_1, file_h);
constexpr int h2 = ce_ind(rank_2, file_h);
constexpr int h3 = ce_ind(rank_3, file_h);
constexpr int h4 = ce_ind(rank_4, file_h);
constexpr int h5 = ce_ind(rank_5, file_h);
constexpr int h6 = ce_ind(rank_6, file_h);
constexpr int h7 = ce_ind(rank_7, file_h);
constexpr int h8 = ce_ind(rank_8, file_h);

}

#endif
