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

#ifndef TALTOS_POSITION_H
#define TALTOS_POSITION_H

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "macros.h"
#include "bitboard.h"
#include "constants.h"
#include "chess.h"
#include "tt.h"
#include "zobrist.h"

enum { pos_alignment = 32 };

#define PIECE_ARRAY_SIZE 14

// make sure the piece emumeration values can be used for indexing such an array
static_assert(pawn >= 2 && pawn < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(king >= 2 && king < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(knight >= 2 && knight < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(rook >= 2 && rook < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(bishop >= 2 && bishop < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(queen >= 2 && queen < PIECE_ARRAY_SIZE, "invalid enum");

struct position {
	alignas(pos_alignment)

	unsigned char board[64];

	/*
	 * The map[0] and map[1] bitboards contain maps of each players
	 * pieces, the rest contain maps of individual pieces.
	 */
	uint64_t map[PIECE_ARRAY_SIZE];

	/*
	 * Thus given a bitboard of a players pawns:
	 *
	 * ........
	 * ........
	 * .......1
	 * ........
	 * ..11....
	 * .....1..
	 * 1.......
	 * ........
	 *
	 * half_open_files:
	 *
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 *
	 * pawn_attack_reach:
	 *
	 * .1111.1.
	 * .1111.1.
	 * .1111.1.
	 * .1111.1.
	 * .1..1.1.
	 * .1......
	 * ........
	 * ........
	 */

	/*
	 * Each square of every file left half open by players pawns,
	 * i.e. where player has no pawn, and another bitboard for
	 * those files where the opponent has no pawns.
	 */
	uint64_t half_open_files[2];

	/*
	 * Each square that can be attacked by pawns, if they are pushed
	 * forward, per player.
	 */
	uint64_t pawn_attack_reach[2];

	uint64_t rays[2][64];
	uint64_t zhash;
	int16_t material_value[2];

	/*
	 * Two booleans corresponding to castling rights, and the sum of piece
	 * values - updated on each move - stored for one playes in 64 bits.
	 * The next 64 bits answer the same questions about the opposing side.
	 */
	int8_t cr_white_king_side;
	int8_t cr_white_queen_side;

	int8_t cr_black_king_side;
	int8_t cr_black_queen_side;

	enum player turn;
	enum player opponent;
	unsigned half_move_counter;
	unsigned full_move_counter;

	alignas(pos_alignment)
	char memcpy_offset;

	/*
	 * In case the player to move is in check:
	 * A bitboard of all pieces that attack the king, and the squares of any
	 * sliding attack between the king and the attacking rook/queen/bishop.
	 * These extra squares are valid move destinations for blocking the
	 * attack, thus moving out of check. E.g. a rook attacking the king:
	 *
	 *    ........
	 *    ..r..K..
	 *    ........
	 *
	 *  The corresponding part of the king_attack_map:
	 *
	 *    ........
	 *    ..XXX...
	 *    ........
	 */
	uint64_t king_attack_map;
	uint64_t king_danger_map;

	/*
	 * En passant index.
	 * Set every time a pawn double-push.
	 */
	// en passant index, even when capture is not legal
	coordinate actual_ep_index;

	/*
	 * Effective en passant index.
	 * Index of a pawn that can be captured en passant.
	 * If there is no such legal capture, ep_index is zero.
	 */
	coordinate ep_index;

	// A bitboard of all pieces
	uint64_t occupied;

	// The square of the king belonging to the player to move
	coordinate white_ki;
	coordinate black_ki;

	/*
	 * The bitboards attack[0] and attack[1] contain maps all squares
	 * attack by each side. Attack maps of each piece type for each side
	 * start from attack[2] --- to be indexed by piece type.
	 */
	uint64_t attack[PIECE_ARRAY_SIZE];

	/*
	 * All sliding attacks ( attacks by bishop, rook, or queen ) of
	 * each player.
	 */
	uint64_t sliding_attacks[2];

	uint64_t rq[2]; // map[rook] | map[queen]
	uint64_t bq[2]; // map[bishop] | map[queen]

	// pinned pieces
	uint64_t king_pins[2];

	// knights and bishops
	uint64_t nb[2];

	// each players pieces not defended by other pieces of the same player
	uint64_t undefended[2];

	uint64_t all_kings;
	uint64_t all_knights;
	uint64_t all_rq;
	uint64_t all_bq;

	alignas(64)
	uint8_t hanging[64];
	uint64_t hanging_map;
};

enum position_ray_directions {
	pr_bishop,
	pr_rook,
};



static inline enum piece
pos_piece_at(const struct position *p, int i)
{
	invariant(ivalid(i));
	return p->board[i];
}

static inline enum player
pos_player_at(const struct position *p, int i)
{
	invariant(ivalid(i));
	return is_nonempty(p->map[black] & bit64(i)) ? black : white;
}

static inline int
pos_square_at(const struct position *p, int i)
{
	invariant(ivalid(i));
	return pos_piece_at(p, i) | pos_player_at(p, i);
}

static inline uint64_t
bishop_reach(const struct position *p, int i)
{
	invariant(ivalid(i));
	return p->rays[pr_bishop][i];
}

static inline uint64_t
rook_reach(const struct position *p, int i)
{
	invariant(ivalid(i));
	return p->rays[pr_rook][i];
}

static inline uint64_t
diag_reach(const struct position *p, int i)
{
	invariant(ivalid(i));
	return bishop_reach(p, i) & diag_masks[i];
}

static inline uint64_t
adiag_reach(const struct position *p, int i)
{
	invariant(ivalid(i));
	return bishop_reach(p, i) & adiag_masks[i];
}

static inline uint64_t
hor_reach(const struct position *p, int i)
{
	invariant(ivalid(i));
	return rook_reach(p, i) & hor_masks[i];
}

static inline uint64_t
ver_reach(const struct position *p, int i)
{
	invariant(ivalid(i));
	return rook_reach(p, i) & ver_masks[i];
}

static inline uint64_t
pos_king_attackers(const struct position *p)
{
	return p->king_attack_map & p->map[opponent_of(p->turn)];
}

static inline uint64_t
absolute_pins(const struct position *p, enum player player)
{
	return p->king_pins[player] & p->map[player];
}

static inline int
pos_has_ep_index(const struct position *p)
{
	return p->ep_index != 0;
}

static inline int
pos_en_passant_file(const struct position *p)
{
	invariant(pos_has_ep_index(p));
	return ind_file(p->ep_index);
}

static inline uint64_t
pos_hash(const struct position *p)
{
	uint64_t key = p->zhash;

	if (pos_has_ep_index(p))
		key = z_toggle_ep_file(key, pos_en_passant_file(p));

	return key;
}

static inline uint64_t
rook_full_attack(int i)
{
	invariant(ivalid(i));
	return file64(i) | rank64(i);
}

static inline bool
is_in_check(const struct position *p)
{
	return is_nonempty(p->king_attack_map);
}

static inline bool
has_insufficient_material(const struct position *p)
{
	// Look at all pieces except kings.
	uint64_t pieces = p->occupied;
	pieces &= ~(p->map[white_king] | p->map[black_king]);

	// Are there only kings and bishops left?
	// This also covers the case where nothing but kings are left.
	if (pieces == (p->map[white_bishop] | p->map[black_bishop])) {
		// All bishops on the same color?
		if (is_empty(pieces & BLACK_SQUARES))
			return true;
		if (is_empty(pieces & WHITE_SQUARES))
			return true;
	}
	else if (is_singular(pieces) && p->board[bsf(pieces)] == knight) {
		return true; // Only a single knight left
	}

	return false;
}

void make_move(struct position *restrict dst,
		const struct position *restrict src,
		struct move)
	attribute(nonnull);

static inline void
pos_switch_turn(struct position *pos)
{
	invariant(pos->turn == white || pos->turn == black);
	invariant(pos->opponent == white || pos->opponent == black);
	invariant(pos->opponent == opponent_of(pos->turn));

	pos->turn ^= (white|black);
	pos->opponent ^= (white|black);

	invariant(pos->opponent == opponent_of(pos->turn));
}

#endif
