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

#ifndef TALTOS_POSITION_H
#define TALTOS_POSITION_H

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>

#include "macros.h"
#include "bitboard.h"
#include "constants.h"
#include "chess.h"
#include "hash.h"

namespace taltos
{

/*
 * PIECE_ARRAY_SIZE - the length of an array that contains one item
 * for each player, followed by one for piece type of each player.
 * At index 0 is an item corresponding to the player next to move,
 * while at index 1 is the similar item corresponding to the opponent.
 * At indices 2, 4, 6, 8, 10, 12 are items corresponding to the players
 * pieces, and at odd indices starting at three are the opponent's items.
 * Such an array can be indexed by an index representing a player ( 0 or 1),
 * or by a piece, e.g. postion.map[pawn] is the map of pawns,
 * position.map[pawn + 1] == position.map[opponent_pawn] is the map of the
 * opponent's pawns.
 */
#define PIECE_ARRAY_SIZE 14

// make sure the piece emumeration values can be used for indexing such an array
static_assert(pawn >= 2 && pawn < PIECE_ARRAY_SIZE);
static_assert(king >= 2 && king < PIECE_ARRAY_SIZE);
static_assert(knight >= 2 && knight < PIECE_ARRAY_SIZE);
static_assert(rook >= 2 && rook < PIECE_ARRAY_SIZE);
static_assert(bishop >= 2 && bishop < PIECE_ARRAY_SIZE);
static_assert(queen >= 2 && queen < PIECE_ARRAY_SIZE);

struct alignas(64) position {
	static void* operator new(size_t);
	static void* operator new[](size_t);

	unsigned char board[64];

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

	uint64_t padding;

	/*
	 * Index of a pawn that can be captured en passant.
	 * If there is no such pawn, ep_index is zero.
	 */
	uint64_t ep_index;

	// A bitboard of all pieces
	uint64_t occupied;

	// The square of the king belonging to the player to move
	int32_t ki;
	int32_t opp_ki;

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

	/*
	 * The map[0] and map[1] bitboards contain maps of each players
	 * pieces, the rest contain maps of individual pieces.
	 */
	uint64_t map[PIECE_ARRAY_SIZE];

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

	alignas(64)
	uint64_t rq[2]; // map[rook] | map[queen]
	uint64_t bq[2]; // map[bishop] | map[queen]

	alignas(64)
	uint64_t rays[2][64];

	alignas(64)
	/*
	 * The following four 64 bit contain two symmetric pairs, that can be
	 * swapped in make_move, as in:
	 *
	 * new->zhash[0] = old->zhash[1]
	 * new->zhash[1] = old->zhash[0]
	 * new->cr_and_material[0] = old->cr_and_material[1]
	 * new->cr_and_material[1] = old->cr_and_material[0]
	 */

	/*
	 * The zobrist hash key of the position is in zhash[0], while
	 * the key from the opponent's point of view is in zhash[1].
	 * During make_move these two are exchanged with each, and both
	 * are updated. This way, there is no need for information on the
	 * current side to move in the hash - actually in the whole
	 * position structure - and can recognize transpositions that
	 * appear with opposite players.
	 */
	uint64_t zhash[2];

	/*
	 * Two booleans corresponding to castling rights, and the sum of piece
	 * values - updated on each move - stored for one playes in 64 bits.
	 * The next 64 bits answer the same questions about the opposing side.
	 */
	int8_t cr_king_side;
	int8_t cr_queen_side;
	int8_t cr_padding0[2];
	int32_t material_value;

	int8_t cr_opponent_king_side;
	int8_t cr_opponent_queen_side;
	int8_t cr_padding1[2];
	int32_t opponent_material_value;

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

static_assert(offsetof(struct position, opponent_material_value) +
	sizeof(((struct position*)NULL)->opponent_material_value) -
	offsetof(struct position, cr_king_side) == 16,
	"struct position layout error");

static_assert(offsetof(struct position, opponent_material_value) +
	sizeof(((struct position*)NULL)->opponent_material_value) -
	offsetof(struct position, zhash) == 32,
	"struct position layout error");

enum position_ray_directions {
	pr_bishop,
	pr_rook,
};



static inline int
pos_piece_at(const struct position *p, int i)
{
	invariant(ivalid(i));
	return p->board[i];
}

static inline int
pos_player_at(const struct position *p, int i)
{
	invariant(ivalid(i));
	return is_nonempty(p->map[1] & bit64(i)) ? 1 : 0;
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
	return p->king_attack_map & p->map[1];
}

static inline uint64_t
absolute_pins(const struct position *p, int player)
{
	return p->king_pins[player] & p->map[player];
}

static inline int
pos_en_passant_file(const struct position *p)
{
	return ind_file(p->ep_index);
}

static inline int
pos_has_ep_target(const struct position *p)
{
	return p->ep_index != 0;
}

static inline uint64_t
pos_hash(const struct position *p)
{
	uint64_t key = p->zhash[0];
	if (pos_has_ep_target(p))
		key = z_toggle_ep_file(key, pos_en_passant_file(p));
	return key;
}

static inline uint64_t
rank64(int i)
{
	invariant(ivalid(i));
	return RANK_8 << (i & 0x38);
}

static inline uint64_t
file64(int i)
{
	invariant(ivalid(i));
	return FILE_H << (i % 8);
}

static inline uint64_t
pawn_reach_south(uint64_t map)
{
	return ((map & ~FILE_H) << 7) | ((map & ~FILE_A) << 9);
}

static inline uint64_t
pawn_reach_north(uint64_t map)
{
	return ((map & ~FILE_A) >> 7) | ((map & ~FILE_H) >> 9);
}

static inline uint64_t
pawn_attacks_opponent(uint64_t pawn_map)
{
	return pawn_reach_south(pawn_map);
}

static inline uint64_t
pawn_attacks_player(uint64_t pawn_map)
{
	return pawn_reach_north(pawn_map);
}

static inline uint64_t
pos_pawn_attacks_player(const struct position *p)
{
	return pawn_attacks_player(p->map[pawn]);
}

static inline uint64_t
pos_pawn_attacks_opponent(const struct position *p)
{
	return pawn_attacks_player(p->map[opponent_pawn]);
}

static inline uint64_t
rook_full_attack(int i)
{
	invariant(ivalid(i));
	return file64(i) | rank64(i);
}

static inline int
pos_king_index_player(const struct position *p)
{
	uint64_t map = p->map[king];
	invariant(map != 0);
	return bsf(map);
}

static inline int
pos_king_index_opponent(const struct position *p)
{
	uint64_t map = p->map[opponent_king];
	invariant(map != 0);
	return bsf(map);
}

static inline uint64_t
pos_king_knight_attack(const struct position *p)
{
	return knight_pattern[p->opp_ki] & p->map[knight];
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
	uint64_t pieces = p->occupied & ~(p->map[king] | p->map[opponent_king]);

	// Are there only kings and bishops left?
	// This also covers the case where nothing but kings are left.
	if (pieces == (p->map[bishop] | p->map[opponent_bishop])) {
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
		move);

}

#endif
