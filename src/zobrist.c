/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
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

#include <assert.h>

#include "macros.h"
#include "hash.h"
#include "position.h"

#include "z_random.inc"

uint64_t
position_polyglot_key(const struct position *pos, enum player turn)
{
	uint64_t key = UINT64_C(0);

	/*
	 * Taltos an polyglot have different table representations,
	 * while Taltos uses the 64 bit constants also used by Polyglot.
	 * Hence using here (7-row) and (7-file) for
	 * indexing the z_random array.
	 */
	for (int row = 0; row < 8; ++row) {
		for (int file = 0; file < 8; ++file) {
			enum piece p = pos_piece_at(pos, ind(row, file));
			enum player pl = pos_player_at(pos, ind(row, file));
			int index;

			if (p == nonpiece)
				continue;

			if (turn == white) {
				pl = opponent_of(pl);
				index = (7 - row) * 8 + (7 - file);
			}
			else {
				index = row * 8 + (7 - file);
			}
			key ^= z_random[p + pl][index];
		}
	}
	if (turn == white) {
		if (position_cr_opponent_queen_side(pos))
			key = z_toggle_castle_queen_side_opponent(key);
		if (position_cr_queen_side(pos))
			key = z_toggle_castle_queen_side(key);
		if (position_cr_opponent_king_side(pos))
			key = z_toggle_castle_king_side_opponent(key);
		if (position_cr_king_side(pos))
			key = z_toggle_castle_king_side(key);
	}
	else {
		if (position_cr_opponent_queen_side(pos))
			key = z_toggle_castle_queen_side(key);
		if (position_cr_queen_side(pos))
			key = z_toggle_castle_queen_side_opponent(key);
		if (position_cr_opponent_king_side(pos))
			key = z_toggle_castle_king_side(key);
		if (position_cr_king_side(pos))
			key = z_toggle_castle_king_side_opponent(key);
	}
	if (pos_has_ep_target(pos)) {
		if (is_nonempty(pos_pawn_attacks_player(pos)
		    & bit64(pos->ep_index + NORTH))) {
			int file = 7 - pos_en_passant_file(pos);
			key = z_toggle_ep_file(key, file);
		}
	}
	if (turn == white)
		key ^= UINT64_C(0xF8D626AAAF278509);
	return key;
}

alignas(16) uint64_t zhash_xor_table[64 * 64 * 8 * 8 * 8][2];

static void
init_zhash_table_mt_general(int from, int to)
{
	for (int p0 = 2; p0 <= 12; p0 += 2) {
		for (int p1 = 0; p1 <= 12; p1 += 2) {
			move m = create_move_g(from, to, p0, p1);
			assert((unsigned)m < ARRAY_LENGTH(zhash_xor_table));

			uint64_t hash = z_random[opponent_of(p0)][from];
			hash ^= z_random[opponent_of(p0)][to];
			hash ^= z_random[p1][to];
			zhash_xor_table[m][0] = hash;

			hash = z_random[p0][flip_i(from)];
			hash ^= z_random[p0][flip_i(to)];
			hash ^= z_random[opponent_of(p1)][flip_i(to)];
			zhash_xor_table[m][1] = hash;
		}
	}
}

static void
init_zhash_table_promotion(int from, int to, int p1)
{
	for (int p0 = 2; p0 <= 12; p0 += 2) {
		move m = create_move_pr(from, to, p0, p1);

		assert((unsigned)m < ARRAY_LENGTH(zhash_xor_table));

		uint64_t hash = z_random[opponent_pawn][from];
		hash ^= z_random[opponent_of(p0)][to];
		hash ^= z_random[p1][to];
		zhash_xor_table[m][0] = hash;

		hash = z_random[pawn][flip_i(from)];
		hash ^= z_random[p0][flip_i(to)];
		hash ^= z_random[opponent_of(p1)][flip_i(to)];
		zhash_xor_table[m][1] = hash;
	}
}

static void
init_zhash_table_double_push(int from, int to)
{
	move m = create_move_pd(from, to);

	assert((unsigned)m < ARRAY_LENGTH(zhash_xor_table));

	uint64_t hash = z_random[opponent_pawn][from];
	hash ^= z_random[opponent_pawn][to];
	zhash_xor_table[m][0] = hash;

	hash = z_random[pawn][flip_i(from)];
	hash ^= z_random[pawn][flip_i(to)];
	zhash_xor_table[m][1] = hash;
}

static void
init_zhash_table_ep(int from, int to)
{
	move m = create_move_ep(from, to);

	assert((unsigned)m < ARRAY_LENGTH(zhash_xor_table));

	uint64_t hash = z_random[opponent_pawn][from];
	hash ^= z_random[opponent_pawn][to];
	hash ^= z_random[pawn][to + NORTH];
	zhash_xor_table[m][0] = hash;

	hash = z_random[pawn][flip_i(from)];
	hash ^= z_random[pawn][flip_i(to)];
	hash ^= z_random[opponent_pawn][flip_i(to + NORTH)];
	zhash_xor_table[m][1] = hash;
}

static void
init_zhash_table_castles(void)
{
	move m = flip_m(mcastle_king_side);
	assert((unsigned)m < ARRAY_LENGTH(zhash_xor_table));

	uint64_t hash = z_random[opponent_king][sq_e8];
	hash ^= z_random[opponent_king][sq_g8];
	hash ^= z_random[opponent_rook][sq_f8];
	hash ^= z_random[opponent_rook][sq_h8];
	zhash_xor_table[m][0] = hash;

	hash = z_random[king][sq_e1];
	hash ^= z_random[king][sq_g1];
	hash ^= z_random[rook][sq_f1];
	hash ^= z_random[rook][sq_h1];
	zhash_xor_table[m][1] = hash;

	m = flip_m(mcastle_queen_side);
	assert((unsigned)m < ARRAY_LENGTH(zhash_xor_table));

	hash = z_random[opponent_king][sq_e8];
	hash ^= z_random[opponent_king][sq_c8];
	hash ^= z_random[opponent_rook][sq_a8];
	hash ^= z_random[opponent_rook][sq_d8];
	zhash_xor_table[m][0] = hash;

	hash = z_random[king][sq_e1];
	hash ^= z_random[king][sq_c1];
	hash ^= z_random[rook][sq_a1];
	hash ^= z_random[rook][sq_d1];
	zhash_xor_table[m][1] = hash;
}

void
init_zhash_table(void)
{
	for (int from = 0; from < 64; ++from) {
		for (int to = 0; to < 64; ++to) {
			init_zhash_table_mt_general(from, to);
		}
	}

	for (int file = file_a; is_valid_file(file); file += EAST) {
		int from = ind(rank_2, file);
		int to = from + SOUTH;
		init_zhash_table_promotion(from, to, 0);
		if (file != file_a) {
			to = from + SOUTH + WEST;
			init_zhash_table_promotion(from, to, rook);
			init_zhash_table_promotion(from, to, knight);
			init_zhash_table_promotion(from, to, bishop);
			init_zhash_table_promotion(from, to, queen);
		}
		if (file != file_h) {
			to = from + SOUTH + EAST;
			init_zhash_table_promotion(from, to, rook);
			init_zhash_table_promotion(from, to, knight);
			init_zhash_table_promotion(from, to, bishop);
			init_zhash_table_promotion(from, to, queen);
		}

		from = ind(rank_7, file);
		init_zhash_table_double_push(from, from + 2 * SOUTH);

		from = ind(rank_4, file);
		if (file != file_a)
			init_zhash_table_ep(from, from + SOUTH + WEST);
		if (file != file_h)
			init_zhash_table_ep(from, from + SOUTH + EAST);
	}

	init_zhash_table_castles();
}
