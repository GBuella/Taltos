
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

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
#include "hash.h"

void setup_registers(void);

#ifdef POSITION_ALIGN_64
enum { pos_alignment = 64 };
#else
enum { pos_alignment = alignof(max_align_t) };
#endif

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
static_assert(pawn >= 2 && pawn < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(king >= 2 && king < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(knight >= 2 && knight < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(rook >= 2 && rook < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(bishop >= 2 && bishop < PIECE_ARRAY_SIZE, "invalid enum");
static_assert(queen >= 2 && queen < PIECE_ARRAY_SIZE, "invalid enum");

struct position {
#ifdef POSITION_ALIGN_64
	alignas(64)
#endif
	char board[64];

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

	// Pins by opponents rooks or queens
	uint64_t rpin_map;

	// Pins by opponents bishops or queens
	uint64_t bpin_map;

	/*
	 * Index of a pawn that can be captured en passant.
	 * If there is no such pawn, ep_index is zero.
	 */
	uint64_t ep_index;

	// A bitboard of all pieces
	uint64_t occupied;

	// The square of the king belonging to the player to move
	uint64_t king_index;

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
};

static_assert(offsetof(struct position, opponent_material_value) +
	sizeof(((struct position*)NULL)->opponent_material_value) -
	offsetof(struct position, cr_king_side) == 16,
	"struct position layout error");

static_assert(offsetof(struct position, opponent_material_value) +
	sizeof(((struct position*)NULL)->opponent_material_value) -
	offsetof(struct position, zhash) == 32,
	"struct position layout error");



static inline void
pos_add_piece(struct position *p, int i, int piece)
{
	extern const int piece_value[PIECE_ARRAY_SIZE];

	invariant(ivalid(i));
	p->board[i] = piece & ~1;
	p->occupied |= bit64(i);
	p->map[piece & 1] |= bit64(i);
	p->map[piece] |= bit64(i);
	p->material_value += piece_value[piece];
	p->opponent_material_value -= piece_value[piece];
}

static inline attribute(artificial) enum piece
pos_piece_at(const struct position *p, int i)
{
	invariant(ivalid(i));
	return p->board[i];
}

static inline attribute(artificial) enum player
pos_player_at(const struct position *p, int i)
{
	invariant(ivalid(i));
	return is_nonempty(p->map[1] & bit64(i)) ? 1 : 0;
}

static inline attribute(artificial) int
pos_square_at(const struct position *p, int i)
{
	invariant(ivalid(i));
	return pos_piece_at(p, i) | pos_player_at(p, i);
}

static inline attribute(artificial) uint64_t
pos_king_attackers(const struct position *p)
{
	return p->king_attack_map & p->map[1];
}

static inline attribute(artificial) int
pos_en_passant_file(const struct position *p)
{
	return ind_file(p->ep_index);
}

static inline attribute(artificial) int
pos_has_ep_target(const struct position *p)
{
	return p->ep_index != 0;
}

static inline attribute(artificial) uint64_t
pos_hash(const struct position *p)
{
	uint64_t key = p->zhash[0];
	if (pos_has_ep_target(p))
		key = z_toggle_ep_file(key, pos_en_passant_file(p));
	return key;
}

static inline attribute(artificial) uint64_t
rank64(int i)
{
	invariant(ivalid(i));
	return RANK_8 << (i & 0x38);
}

static inline attribute(artificial) uint64_t
file64(int i)
{
	invariant(ivalid(i));
	return FILE_H << (i % 8);
}

static inline attribute(artificial) uint64_t
sliding_map(uint64_t occ, const struct magical *magic)
{
	uint64_t index = ((occ & magic->mask) * magic->multiplier);
	index >>= magic->shift;

#ifdef SLIDING_BYTE_LOOKUP
	return magic->attack_table[magic->attack_index_table[index]];
#else
	return magic->attack_table[index];
#endif
}

static inline attribute(artificial) uint64_t
knight_pattern(int i)
{
	invariant(ivalid(i));

#ifdef USE_KNIGHT_LOOKUP_TABLE
	return knight_moves_table[i];
#else
	static const uint64_t pattern = UINT64_C(0x0442800000028440);
	static const uint64_t mask = UINT64_C(0x00003f3f3f3f3f3f);
	return rol(pattern, i) & (mask << ((i & 0x24) >> 1));
#endif
}

static inline attribute(artificial) uint64_t
pawn_attacks_opponent(uint64_t pawn_map)
{
	return ((pawn_map & ~FILE_H) << 7) | ((pawn_map & ~FILE_A) << 9);
}

static inline attribute(artificial) uint64_t
pawn_attacks_player(uint64_t pawn_map)
{
	return ((pawn_map & ~FILE_A) >> 7) | ((pawn_map & ~FILE_H) >> 9);
}

static inline attribute(artificial) uint64_t
pos_pawn_attacks_player(const struct position *p)
{
	return pawn_attacks_player(p->map[pawn]);
}

static inline attribute(artificial) uint64_t
pos_pawn_attacks_opponent(const struct position *p)
{
	return pawn_attacks_player(p->map[opponent_pawn]);
}

static inline attribute(artificial) uint64_t
rook_full_attack(int i)
{
	invariant(ivalid(i));
	return file64(i) | rank64(i);
}

static inline attribute(artificial) int
pos_king_index_player(const struct position *p)
{
	uint64_t map = p->map[king];
	invariant(map != 0);
	return bsf(map);
}

static inline attribute(artificial) int
pos_king_index_opponent(const struct position *p)
{
	uint64_t map = p->map[opponent_king];
	invariant(map != 0);
	return bsf(map);
}

static inline attribute(artificial) uint64_t
pos_king_knight_attack(const struct position *p)
{
	return knight_pattern(pos_king_index_opponent(p)) & p->map[knight];
}

static inline attribute(artificial) bool
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
		move)
	attribute(nonnull);

#endif
