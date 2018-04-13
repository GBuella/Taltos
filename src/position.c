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

#include <stdlib.h>
#include <string.h>

#include "position.h"
#include "constants.h"
#include "zobrist.h"
#include "util.h"
#include "eval.h"
#include "move_desc.h"

#include "bitboard_constants.inc"


/* Little getter functions, used in bookkeeping - non performance critical */

enum player
position_turn(const struct position *pos)
{
	return pos->turn;
}

bool
pos_is_check(const struct position *pos)
{
	return is_in_check(pos);
}

unsigned
position_full_move_count(const struct position *pos)
{
	return pos->full_move_counter;
}

unsigned
position_half_move_count(const struct position *pos)
{
	return pos->half_move_counter;
}

bool
pos_has_insufficient_material(const struct position *pos)
{
	return has_insufficient_material(pos);
}

bool
pos_equal(const struct position *a, const struct position *b)
{
	for (unsigned i = 0; i < 64; ++i) {
		if (a->board[i] != b->board[i])
			return false;
	}

	if (a->map[white] != b->map[white])
		return false;
	if (a->turn != b->turn)
		return false;
	if (a->ep_index != b->ep_index)
		return false;
	if (a->cr_white_king_side != b->cr_white_king_side)
		return false;
	if (a->cr_white_queen_side != b->cr_white_queen_side)
		return false;
	if (a->cr_black_king_side != b->cr_black_king_side)
		return false;
	if (a->cr_black_queen_side != b->cr_black_queen_side)
		return false;

	return true;
}

int
position_square_at(const struct position *pos, coordinate index)
{
	return pos_square_at(pos, index);
}

enum piece
position_piece_at(const struct position *pos, coordinate index)
{
	return pos_piece_at(pos, index);
}

enum player
position_player_at(const struct position *pos, coordinate index)
{
	return pos_player_at(pos, index);
}

bool
position_has_en_passant_index(const struct position *pos)
{
	return pos->ep_index != 0;
}

bool
position_has_en_passant_target(const struct position *pos)
{
	return pos->actual_ep_index != 0;
}

int
position_get_en_passant_index(const struct position *pos)
{
	assert(pos_has_ep_index(pos));
	return pos->ep_index;
}

int
position_get_en_passant_target(const struct position *pos)
{
	assert(position_has_en_passant_target(pos));
	if (pos->turn == white)
		return pos->actual_ep_index + NORTH;
	else
		return pos->actual_ep_index + SOUTH;
}

bool
position_cr_white_king_side(const struct position *pos)
{
	return pos->cr_white_king_side;
}

bool
position_cr_white_queen_side(const struct position *pos)
{
	return pos->cr_white_queen_side;
}

bool
position_cr_black_king_side(const struct position *pos)
{
	return pos->cr_black_king_side;
}

bool
position_cr_black_queen_side(const struct position *pos)
{
	return pos->cr_black_queen_side;
}

uint64_t
get_position_key(const struct position *pos)
{
	return pos_hash(pos);
}

static bool
has_pawn_east_of(const struct position *pos, coordinate i)
{
	if (ind_file(i) == file_a)
		return false;

	return is_nonempty(pos->map[pos->turn + pawn] & bit64(i + WEST));
}

static bool
has_pawn_west_of(const struct position *pos, coordinate i)
{
	if (ind_file(i) == file_h)
		return false;

	return is_nonempty(pos->map[pos->turn + pawn] & bit64(i + EAST));
}

static bool
has_potential_ep_captor(struct position *pos, coordinate index)
{
	return has_pawn_east_of(pos, index) || has_pawn_west_of(pos, index);
}


/*
 * Setup the attack bitboards.
 * One bitboard per each piece type/side, to represent
 * the squares attacked by thath piece.
 * Opponents sliding pieces are special cases, their attacks
 * maps are calculated as if they could jump over the king of the side to move.
 * This makes it easier to find squares where a king in check can move to.
 */
static void generate_all_rays(struct position*);
static void update_all_rays(struct position*, struct move);
static void generate_white_attacks(struct position*);
static void generate_black_attacks(struct position*);
static void search_king_attacks(struct position*);
static void search_pins(struct position*);

static void
search_player_king_pins(struct position *pos, int player)
{
	uint64_t accumulator = EMPTY;
	int ki = bsf(pos->map[king + player]);
	uint64_t bandits = bishop_masks[ki] & pos->bq[opponent_of(player)];
	bandits &= ~bishop_reach(pos, ki);

	for (; is_nonempty(bandits); bandits = reset_lsb(bandits)) {
		uint64_t bandit_reach = bishop_reach(pos, bsf(bandits));
		uint64_t between = bishop_reach(pos, ki) & bandit_reach;
		between &= pos->occupied;

		if (is_singular(between))
			accumulator |= between;
	}

	bandits = rook_masks[ki] & pos->rq[opponent_of(player)];
	bandits &= ~rook_reach(pos, ki);

	for (; is_nonempty(bandits); bandits = reset_lsb(bandits)) {
		uint64_t bandit_reach = rook_reach(pos, bsf(bandits));
		uint64_t between = rook_reach(pos, ki) & bandit_reach;
		between &= pos->occupied;

		if (is_singular(between))
			accumulator |= between;
	}

	pos->king_pins[player] = accumulator;
}

static void attribute(always_inline)
search_pins(struct position *pos)
{
	search_player_king_pins(pos, white);
	search_player_king_pins(pos, black);
}

static void
generate_rays(struct position *pos, uint64_t rays[64], int dir, uint64_t edge)
{
	for (int from = 0; from < 64; ++from) {
		uint64_t bit = bit64(from);
		if (is_nonempty(edge & bit))
			continue;

		uint64_t stop = edge | pos->occupied;
		int i = from;

		do {
			i += dir;
			bit = bit64(i);
			rays[from] |= bit;
		} while (is_empty(stop & bit));
	}
}

static void
generate_all_rays(struct position *pos)
{
	memset(pos->rays, 0, sizeof(pos->rays));

	generate_rays(pos, pos->rays[pr_rook], WEST, FILE_A);
	generate_rays(pos, pos->rays[pr_rook], EAST, FILE_H);
	generate_rays(pos, pos->rays[pr_rook], NORTH, RANK_8);
	generate_rays(pos, pos->rays[pr_rook], SOUTH, RANK_1);
	generate_rays(pos, pos->rays[pr_bishop], SOUTH + EAST, RANK_1 | FILE_H);
	generate_rays(pos, pos->rays[pr_bishop], NORTH + WEST, RANK_8 | FILE_A);
	generate_rays(pos, pos->rays[pr_bishop], SOUTH + WEST, RANK_1 | FILE_A);
	generate_rays(pos, pos->rays[pr_bishop], NORTH + EAST, RANK_8 | FILE_H);
}

static void
update_rays_empty_square(uint64_t rays[64], const uint64_t masks[64], int i)
{
	uint64_t reach = rays[i];
	for (uint64_t map = reach; is_nonempty(map); map = reset_lsb(map))
		rays[bsf(map)] |= reach & masks[bsf(map)];
}

static void
update_all_rays_empty_square(struct position *pos, int i)
{
	update_rays_empty_square(pos->rays[pr_rook], rook_masks, i);
	update_rays_empty_square(pos->rays[pr_bishop], bishop_masks, i);
}

static void
update_rays_occupied_square(uint64_t rays[64], int i)
{
	uint64_t lower_half = rays[i] & (bit64(i) - 1);
	uint64_t higher_half = rays[i] & ~lower_half;

	for (uint64_t map = lower_half; is_nonempty(map); map = reset_lsb(map))
		rays[bsf(map)] &= ~higher_half;

	for (uint64_t map = higher_half; is_nonempty(map); map = reset_lsb(map))
		rays[bsf(map)] &= ~lower_half;
}

static void
update_all_rays_occupied_square(struct position *pos, int i)
{
	update_rays_occupied_square(pos->rays[pr_rook], i);
	update_rays_occupied_square(pos->rays[pr_bishop], i);
}

static void
update_all_rays(struct position *pos, struct move m)
{
	update_all_rays_empty_square(pos, m.from);
	if (m.type == mt_en_passant) {
		update_all_rays_occupied_square(pos, m.to);
		if (pos->turn == white)
			update_all_rays_empty_square(pos, m.to + NORTH);
		else
			update_all_rays_empty_square(pos, m.to + SOUTH);
	}
	else if (!is_capture(m)) {
		update_all_rays_occupied_square(pos, m.to);
		if (move_eq(m, mcastle_white_king_side()))
			update_all_rays_occupied_square(pos, f1);
		else if (move_eq(m, mcastle_white_queen_side()))
			update_all_rays_occupied_square(pos, d1);
		if (move_eq(m, mcastle_black_king_side()))
			update_all_rays_occupied_square(pos, f8);
		else if (move_eq(m, mcastle_black_queen_side()))
			update_all_rays_occupied_square(pos, d8);
	}
}

static uint64_t
bishop_attacks(uint64_t bishops, const struct position *pos)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(bishops); bishops = reset_lsb(bishops))
		accumulator |= bishop_reach(pos, bsf(bishops));

	return accumulator;
}

static uint64_t
rook_attacks(uint64_t rooks, const struct position *pos)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(rooks); rooks = reset_lsb(rooks))
		accumulator |= rook_reach(pos, bsf(rooks));

	return accumulator;
}

static uint64_t
knight_attacks(uint64_t knights)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(knights); knights = reset_lsb(knights))
		accumulator |= knight_pattern[bsf(knights)];

	return accumulator;
}

static void
accumulate_attacks(uint64_t *attack, uint64_t *sliding_attacks)
{
	*sliding_attacks = attack[bishop];
	*sliding_attacks |= attack[rook];
	*sliding_attacks |= attack[queen];
	attack[0] = attack[pawn];
	attack[0] |= attack[knight];
	attack[0] |= attack[king];
	attack[0] |= *sliding_attacks;
}

static void attribute(always_inline)
generate_white_attacks(struct position *pos)
{
	pos->attack[white_pawn] = white_pawn_attacks(pos->map[white_pawn]);
	pos->attack[white_king] = king_pattern[pos->white_ki];
	pos->attack[white_knight] = knight_attacks(pos->map[white_knight]);
	pos->attack[white_bishop] = bishop_attacks(pos->map[white_bishop], pos);
	pos->attack[white_rook] = rook_attacks(pos->map[white_rook], pos);
	pos->attack[white_queen] = bishop_attacks(pos->map[white_queen], pos);
	pos->attack[white_queen] |= rook_attacks(pos->map[white_queen], pos);
	accumulate_attacks(pos->attack + white, pos->sliding_attacks + white);
}

static void attribute(always_inline)
generate_black_attacks(struct position *pos)
{
	pos->attack[black_pawn] = black_pawn_attacks(pos->map[black_pawn]);
	pos->attack[black_king] = king_pattern[pos->black_ki];
	pos->attack[black_knight] = knight_attacks(pos->map[black_knight]);
	pos->attack[black_bishop] = bishop_attacks(pos->map[black_bishop], pos);
	pos->attack[black_rook] = rook_attacks(pos->map[black_rook], pos);
	pos->attack[black_queen] = bishop_attacks(pos->map[black_queen], pos);
	pos->attack[black_queen] |= rook_attacks(pos->map[black_queen], pos);
	accumulate_attacks(pos->attack + black, pos->sliding_attacks + black);
}

static void
search_king_attacks(struct position *pos)
{
	pos->king_danger_map = EMPTY;
	int ki;

	if (pos->turn == white) {
		ki = pos->white_ki;
		pos->king_attack_map = white_pawn_attacks(
			pos->map[white_king]) & pos->map[black_pawn];
	}
	else {
		ki = pos->black_ki;
		pos->king_attack_map = black_pawn_attacks(
			pos->map[black_king]) & pos->map[white_pawn];
	}

	pos->king_attack_map |=
	    knight_pattern[ki] & pos->map[pos->opponent | knight];

	uint64_t breach = bishop_reach(pos, ki);
	uint64_t bandits = breach & pos->bq[pos->opponent];
	for (; is_nonempty(bandits); bandits = reset_lsb(bandits)) {
		uint64_t bandit = lsb(bandits);
		uint64_t reach = breach;

		if (is_nonempty(bandit & diag_masks[ki]))
			reach &= diag_masks[ki];
		else
			reach &= adiag_masks[ki];

		pos->king_danger_map |= reach & ~bandit;

		reach &= bishop_reach(pos, bsf(bandit));
		pos->king_attack_map |= reach | bandit;
	}

	uint64_t rreach = rook_reach(pos, ki);
	bandits = rreach & pos->rq[pos->opponent];
	for (; is_nonempty(bandits); bandits = reset_lsb(bandits)) {
		uint64_t bandit = lsb(bandits);
		uint64_t reach = rreach;

		if (is_nonempty(bandit & hor_masks[ki]))
			reach &= hor_masks[ki];
		else
			reach &= ver_masks[ki];

		pos->king_danger_map |= reach & ~bandit;

		reach &= rook_reach(pos, bsf(bandit));
		pos->king_attack_map |= reach | bandit;
	}
}


// Generate the pawn_attack_reach, and half_open_files bitboards

static void
generate_white_pawn_reach_maps(struct position *pos)
{
	uint64_t reach = kogge_stone_north(north_of(pos->map[white_pawn]));

	pos->half_open_files[white] = ~kogge_stone_south(reach);
	pos->pawn_attack_reach[white] =
	    (west_of(reach) & ~FILE_H) | (east_of(reach) & ~FILE_A);
}

static void
generate_black_pawn_reach_maps(struct position *pos)
{
	uint64_t reach = kogge_stone_south(south_of(pos->map[black_pawn]));

	pos->half_open_files[black] = ~kogge_stone_north(reach);
	pos->pawn_attack_reach[black] =
	    (west_of(reach) & ~FILE_H) | (east_of(reach) & ~FILE_A);
}

static void
generate_pawn_reach_maps(struct position *pos)
{
	generate_white_pawn_reach_maps(pos);
	generate_black_pawn_reach_maps(pos);
}


/*
 * Utility functions used in setting up a new position from scratch
 * non performance critical.
 * The function position_reset is for setting up an already allocated position,
 * based on raw data -- only comes from FEN currently.
 * The function position_create also allocates.
 * The function position_allocate does just allocate an unintialized struct.
 */

// setup the Zobrist hash key from scratch
static attribute(nonnull) void setup_zhash(struct position*);

// setup the board, and corresponding bitboards from scratch
static int board_reset(struct position*, const char board[static 64]);

// Check for too many pawns, pawns on wrong rank
static bool pawns_valid(const struct position*);

// Check if the given square has pawn residing on it, which was possibly just
// recently pushed there with a double push
static bool can_be_valid_ep_index(const struct position*, coordinate index);

// Checks placements of kings and rooks being consistent with castling rights
static bool castle_rights_valid(const struct position*);

// Checks for valid placement - and existence - of the two kings
static bool kings_valid(const struct position*);

static bool kings_attacks_valid(const struct position*);

static void accumulate_occupancy(struct position*);

static void accumulate_misc_patterns(struct position*, int player);

int
position_reset(struct position *pos, struct position_desc desc)
{
	struct position dummy;

	/*
	 * A NULL pointer argument means the caller merely needs to validate
	 * the data, and is only interested in the return value of
	 * position_reset.  * A dummy position is set up on the stack, so it
	 * is going to be discarded upon returning from position_reset.
	 */
	if (pos == NULL)
		pos = &dummy;

	memset(pos, 0, sizeof(*pos));
	if (board_reset(pos, desc.board) != 0)
		return -1;
	pos->turn = desc.turn;
	pos->opponent = opponent_of(pos->turn);
	accumulate_occupancy(pos);
	pos->cr_white_king_side = desc.castle_rights[cri_white_king_side];
	pos->cr_white_queen_side = desc.castle_rights[cri_white_queen_side];
	pos->cr_black_king_side = desc.castle_rights[cri_black_king_side];
	pos->cr_black_queen_side = desc.castle_rights[cri_black_queen_side];
	if (desc.en_passant_index != 0) {
		if (!can_be_valid_ep_index(pos, desc.en_passant_index))
			return -1;
	}
	pos->actual_ep_index = desc.en_passant_index;
	if (popcnt(pos->map[white]) > 16)
		return -1;
	if (popcnt(pos->map[black]) > 16)
		return -1;
	if (!kings_valid(pos))
		return -1;
	pos->white_ki = bsf(pos->map[white_king]);
	pos->black_ki = bsf(pos->map[black_king]);
	if (!pawns_valid(pos))
		return -1;
	if (!castle_rights_valid(pos))
		return -1;
	generate_all_rays(pos);
	generate_white_attacks(pos);
	generate_black_attacks(pos);
	generate_pawn_reach_maps(pos);
	if (!kings_attacks_valid(pos))
		return -1;
	search_king_attacks(pos);
	search_pins(pos);
	accumulate_misc_patterns(pos, white);
	accumulate_misc_patterns(pos, black);
	setup_zhash(pos);
	find_hanging_pieces(pos);
	pos->full_move_counter = desc.full_move_counter;
	pos->half_move_counter = desc.half_move_counter;
	if (pos->actual_ep_index != 0) {
		if (has_potential_ep_captor(pos, pos->actual_ep_index))
			pos->ep_index = desc.en_passant_index;
	}
	return 0;
}

struct position*
position_allocate(void)
{
	struct position *pos = xaligned_alloc(pos_alignment, sizeof *pos);
	memset(pos, 0, sizeof *pos);
	return pos;
}

struct position*
position_create(struct position_desc desc)
{
	struct position *pos = xaligned_alloc(pos_alignment, sizeof *pos);
	if (position_reset(pos, desc) == 0) {
		return pos;
	}
	else {
		xaligned_free(pos);
		return NULL;
	}
}

struct position*
position_dup(const struct position *pos)
{
	struct position *new = xaligned_alloc(pos_alignment, sizeof *new);

	*new = *pos;
	return new;
}

void
position_destroy(struct position *p)
{
	xaligned_free(p);
}

static attribute(nonnull) void
setup_zhash(struct position *pos)
{
	pos->zhash = 0;
	for (uint64_t occ = pos->occupied;
	    is_nonempty(occ);
	    occ = reset_lsb(occ)) {
		int i = bsf(occ);
		pos->zhash = z_toggle_pp(pos->zhash, i,
		    pos_piece_at(pos, i),
		    pos_player_at(pos, i));
	}
	if (pos->cr_white_king_side)
		pos->zhash = z_toggle_white_castle_king_side(pos->zhash);
	if (pos->cr_white_queen_side)
		pos->zhash = z_toggle_white_castle_queen_side(pos->zhash);
	if (pos->cr_black_king_side)
		pos->zhash = z_toggle_black_castle_king_side(pos->zhash);
	if (pos->cr_black_queen_side)
		pos->zhash = z_toggle_black_castle_queen_side(pos->zhash);
}

static bool
kings_valid(const struct position *pos)
{
	// Are there exactly one of each king?
	if (!is_singular(pos->map[white_king]))
		return false;
	if (!is_singular(pos->map[black_king]))
		return false;

	// Are the two kings too close?
	int k0 = bsf(pos->map[white_king]);
	uint64_t k1 = pos->map[black_king];
	if (is_nonempty(king_pattern[k0] & k1))
		return false;

	return true;
}

static bool
kings_attacks_valid(const struct position *pos)
{
	uint64_t rq_attackers;
	int ki;

	if (is_nonempty(pos->attack[pos->turn] &
			pos->map[pos->opponent | king]))
		return false;

	uint64_t k = pos->map[pos->turn | king];

	if (pos->turn == white) {
		ki = pos->white_ki;

		if (popcnt(pawn_reach_north(k) & pos->map[black_pawn]) > 1)
			return false;

		if (popcnt(bishop_reach(pos, ki) & pos->bq[black]) > 1)
			return false;

		rq_attackers = rook_reach(pos, ki) & pos->rq[black];

		if (popcnt(rq_attackers & ~RANK_1) > 1)
			return false;
	}
	else {
		ki = pos->black_ki;

		if (popcnt(pawn_reach_south(k) & pos->map[white_pawn]) > 1)
			return false;

		if (popcnt(bishop_reach(pos, ki) & pos->bq[white]) > 1)
			return false;

		rq_attackers = rook_reach(pos, ki) & pos->rq[white];

		if (popcnt(rq_attackers & ~RANK_8) > 1)
			return false;
	}

	if (popcnt(rq_attackers) > 2)
		return false;

	if (popcnt(knight_pattern[ki] & pos->map[pos->opponent | knight]) > 1)
		return false;

	return true;
}

static bool
pawns_valid(const struct position *pos)
{
	// Each side can have up to 8 pawns
	if (popcnt(pos->map[white_pawn]) > 8)
		return false;
	if (popcnt(pos->map[black_pawn]) > 8)
		return false;

	// No pawn can reside on rank #1 or #8
	if (is_nonempty(pos->map[white_pawn] & (RANK_1 | RANK_8)))
		return false;

	if (is_nonempty(pos->map[black_pawn] & (RANK_1 | RANK_8)))
		return false;

	return true;
}

static bool
castle_rights_valid(const struct position *pos)
{
	if (pos->cr_white_king_side) {
		if (pos_square_at(pos, e1) != white_king
		    || pos_square_at(pos, h1) != white_rook)
			return false;
	}
	if (pos->cr_white_queen_side) {
		if (pos_square_at(pos, e1) != white_king
		    || pos_square_at(pos, a1) != white_rook)
			return false;
	}
	if (pos->cr_black_king_side) {
		if (pos_square_at(pos, e8) != black_king
		    || pos_square_at(pos, h8) != black_rook)
			return false;
	}
	if (pos->cr_black_queen_side) {
		if (pos_square_at(pos, e8) != black_king
		    || pos_square_at(pos, a8) != black_rook)
			return false;
	}
	return true;
}

static void
pos_add_piece(struct position *pos, int i, int square)
{
	extern const int piece_value[PIECE_ARRAY_SIZE];

	enum piece piece = square_piece(square);
	invariant(ivalid(i));
	pos->board[i] = piece;
	pos->occupied |= bit64(i);
	pos->map[square_player(square)] |= bit64(i);
	pos->map[square] |= bit64(i);

	if (piece != king) {
		pos->material_value[square_player(square)] +=
		    piece_value[piece];
	}
}

static void attribute(always_inline)
accumulate_occupancy(struct position *pos)
{
	pos->nb[white] = pos->map[white_knight] | pos->map[white_bishop];
	pos->nb[black] = pos->map[black_knight] | pos->map[black_bishop];
	pos->rq[white] = pos->map[white_rook] | pos->map[white_queen];
	pos->rq[black] = pos->map[black_rook] | pos->map[black_queen];
	pos->all_rq = pos->rq[white] | pos->rq[black];
	pos->bq[white] = pos->map[white_bishop] | pos->map[white_queen];
	pos->bq[black] = pos->map[black_bishop] | pos->map[black_queen];
	pos->all_bq = pos->bq[white] | pos->bq[black];

	pos->map[white] = pos->map[white_pawn];
	pos->map[white] |= pos->map[white_knight];
	pos->map[white] |= pos->map[white_king];
	pos->map[white] |= pos->rq[white] | pos->bq[white];

	pos->map[black] = pos->map[black_pawn];
	pos->map[black] |= pos->map[black_knight];
	pos->map[black] |= pos->map[black_king];
	pos->map[black] |= pos->rq[black] | pos->bq[black];

	pos->all_kings = pos->map[white_king] | pos->map[black_king];
	pos->all_knights = pos->map[white_knight] | pos->map[black_knight];

	pos->occupied = pos->map[white] | pos->map[black];
}

static void
accumulate_misc_patterns(struct position *pos, int player)
{
	pos->undefended[player] = pos->map[player] & ~pos->attack[player];
}

static int
board_reset(struct position *pos, const char board[static 64])
{
	for (int i = 0; i < 64; ++i) {
		if (board[i] != 0) {
			if (!is_valid_square(board[i]))
				return 1;
			pos_add_piece(pos, i, board[i]);
		}
	}
	return 0;
}

static bool
can_be_valid_ep_index(const struct position *pos, coordinate index)
{
	uint64_t bit = bit64(index);

	if (pos->turn == white) {
		if (ind_rank(index) != rank_5)
			return false;
		if (is_empty(pos->map[black_pawn] & bit))
			return false;
		if (is_nonempty(pos->occupied & north_of(bit)))
			return false;
		if (is_nonempty(pos->occupied & north_of(north_of(bit))))
			return false;
	}
	else {
		if (ind_rank(index) != rank_4)
			return false;
		if (is_empty(pos->map[white_pawn] & bit))
			return false;
		if (is_nonempty(pos->occupied & south_of(bit)))
			return false;
		if (is_nonempty(pos->occupied & south_of(south_of(bit))))
			return false;
	}

	return true;
}


static void
adjust_castle_rights(struct position *pos, struct move m)
{
	if (pos->cr_white_king_side) {
		if (m.from == e1 || m.from == h1 || m.to == h1) {
			pos->cr_white_king_side = false;
			pos->zhash =
			    z_toggle_white_castle_king_side(pos->zhash);
		}
	}

	if (pos->cr_white_queen_side) {
		if (m.from == e1 || m.from == a1 || m.to == a1) {
			pos->cr_white_queen_side = false;
			pos->zhash =
			    z_toggle_white_castle_queen_side(pos->zhash);
		}
	}

	if (pos->cr_black_king_side) {
		if (m.from == e8 || m.from == h8 || m.to == h8) {
			pos->cr_black_king_side = false;
			pos->zhash =
			    z_toggle_black_castle_king_side(pos->zhash);
		}
	}

	if (pos->cr_black_queen_side) {
		if (m.from == e8 || m.from == a8 || m.to == a8) {
			pos->cr_black_queen_side = false;
			pos->zhash =
			    z_toggle_black_castle_queen_side(pos->zhash);
		}
	}
}



bool
is_legal_move(const struct position *pos, struct move m)
{
	struct move moves[MOVE_ARRAY_LENGTH];

	(void) gen_moves(pos, moves);
	for (unsigned i = 0; !is_null_move(moves[i]); ++i) {
		if (move_eq(moves[i], m))
			return true;
	}
	return false;
}

bool
is_move_irreversible(const struct position *pos, struct move m)
{
	return (m.type != mt_general)
	    || (m.result == pawn)
	    || (m.captured != 0)
	    || (m.from == a1 && pos->cr_white_queen_side)
	    || (m.from == h1 && pos->cr_white_king_side)
	    || (m.from == e1 && pos->cr_white_queen_side)
	    || (m.from == e1 && pos->cr_white_king_side)
	    || (m.from == a8 && pos->cr_black_queen_side)
	    || (m.from == h8 && pos->cr_black_king_side)
	    || (m.from == e8 && pos->cr_black_queen_side)
	    || (m.from == e8 && pos->cr_black_king_side);
}

bool
has_any_legal_move(const struct position *pos)
{
	struct move moves[MOVE_ARRAY_LENGTH];
	return gen_moves(pos, moves) != 0;
}

static void
clear_from_square(struct position *pos, coordinate i)
{
	invariant(pos->board[i] != 0);
	invariant(is_nonempty(pos->map[pos->turn] & bit64(i)));

	enum piece p = pos->board[i];
	pos->material_value[pos->turn] -= piece_value[p];
	pos->zhash = z_toggle_pp(pos->zhash, i, p, pos->turn);
	pos->map[p + pos->turn] &= ~bit64(i);
//	pos->map[pos->turn] &= ~bit64(i);
	pos->board[i] = 0;
}

static void
clear_captured(struct position *restrict dst,
	       const struct position *restrict src,
	       struct move m)
{
	invariant(is_capture(m));
	coordinate i;

	if (m.type == mt_en_passant)
		i = src->ep_index;
	else
		i = m.to;

	invariant(dst->board[i] == m.captured);

	dst->board[i] = 0;
	dst->material_value[src->opponent] -= piece_value[m.captured];
	dst->zhash = z_toggle_pp(dst->zhash, i, m.captured, src->opponent);
	dst->map[m.captured + src->opponent] &= ~bit64(i);
//	dst->map[src->opponent] &= ~bit64(i);
}

static void
set_to_square(struct position *pos, coordinate i, enum piece piece)
{
	pos->board[i] = piece;
	pos->map[piece + pos->turn] |= bit64(i);
//	pos->map[pos->turn] |= bit64(i);
	pos->zhash = z_toggle_pp(pos->zhash, i, piece, pos->turn);
	pos->material_value[pos->turn] += piece_value[piece];
}

static void
adjust_move_counters(struct position *pos, struct move m)
{
	if (pos->board[m.to] == pawn || is_capture(m))
		pos->half_move_counter = 0;
	else
		pos->half_move_counter++;

	if (pos->turn == black)
		pos->full_move_counter++;
}

static void
setup_en_passant_index(struct position *pos, struct move m)
{
	if (m.type == mt_pawn_double_push) {
		pos->actual_ep_index = m.to;
		if (has_potential_ep_captor(pos, m.to))
			pos->ep_index = m.to;
		else
			pos->ep_index = 0;
	}
	else {
		pos->actual_ep_index = 0;
		pos->ep_index = 0;
	}
}

void
make_move(struct position *restrict dst attribute(align_value(pos_alignment)),
	  const struct position *restrict src attribute(align_value(pos_alignment)),
	  struct move m)
{
	for (size_t i = 0; i < offsetof(struct position, memcpy_offset); ++i)
		((char*)dst)[i] = ((const char*)src)[i];

	/*
	__m256i* dst256 = (__m256i*)dst;
	const __m256i* src256 = (const __m256i*)src;

	for (size_t i = 0;
	     i < (offsetof(struct position, memcpy_offset) / sizeof(*dst256));
	     ++i) {
		_mm256_store_si256(dst256 + i, _mm256_load_si256(src256 + i));
	}
	*/

	/*
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"

	for (size_t i = 0;
	     i < offsetof(struct position, memcpy_offset);
	     i += 256) {
		asm("vmovaps (%0), %%ymm0\n\t"
		    "vmovaps 32(%0), %%ymm1\n\t"
		    "vmovaps 64(%0), %%ymm2\n\t"
		    "vmovaps 96(%0), %%ymm3\n\t"
		    "vmovaps %%ymm3, 96(%1)\n\t"
		    "vmovaps %%ymm2, 64(%1)\n\t"
		    "vmovaps %%ymm1, 32(%1)\n\t"
		    "vmovaps %%ymm0, (%1)\n\t"
		    "vmovaps 128(%0), %%ymm0\n\t"
		    "vmovaps 160(%0), %%ymm1\n\t"
		    "vmovaps 192(%0), %%ymm2\n\t"
		    "vmovaps 224(%0), %%ymm3\n\t"
		    "vmovaps %%ymm3, 224(%1)\n\t"
		    "vmovaps %%ymm2, 192(%1)\n\t"
		    "vmovaps %%ymm1, 160(%1)\n\t"
		    "vmovaps %%ymm0, 128(%1)\n\t"

		    :
		    : "r" (((const char*)src) + i), "r" (((char*)dst) + i)
		    : "ymm0", "ymm1", "ymm2", "ymm3");
	}

#pragma clang diagnostic pop

*/
	/*
	asm("vmovaps (%0), %%ymm0\n\t"
            "vmovaps 32(%0), %%ymm1\n\t"
            "vmovaps 64(%0), %%ymm2\n\t"
            "vmovaps 96(%0), %%ymm3\n\t"
            "vmovaps %%ymm3, 96(%1)\n\t"
            "vmovaps %%ymm2, 64(%1)\n\t"
            "vmovaps %%ymm1, 32(%1)\n\t"
            "vmovaps %%ymm0, (%1)\n\t"

	    "vmovaps 128(%0), %%ymm0\n\t"
            "vmovaps 160(%0), %%ymm1\n\t"
            "vmovaps 192(%0), %%ymm2\n\t"
            "vmovaps 224(%0), %%ymm3\n\t"
            "vmovaps %%ymm3, 224(%1)\n\t"
            "vmovaps %%ymm2, 192(%1)\n\t"
            "vmovaps %%ymm1, 160(%1)\n\t"
            "vmovaps %%ymm0, 128(%1)\n\t"

	    "vmovaps 256(%0), %%ymm0\n\t"
            "vmovaps 160(%0), %%ymm1\n\t"
            "vmovaps 288(%0), %%ymm2\n\t"
            "vmovaps 352(%0), %%ymm3\n\t"
            "vmovaps %%ymm3, 352(%1)\n\t"
            "vmovaps %%ymm2, 320(%1)\n\t"
            "vmovaps %%ymm1, 288(%1)\n\t"
            "vmovaps %%ymm0, 256(%1)\n\t"

	    "vmovaps 384(%0), %%ymm0\n\t"
            "vmovaps 400(%0), %%ymm1\n\t"
            "vmovaps 416(%0), %%ymm2\n\t"
            "vmovaps 432(%0), %%ymm3\n\t"
            "vmovaps %%ymm3, 432(%1)\n\t"
            "vmovaps %%ymm2, 416(%1)\n\t"
            "vmovaps %%ymm1, 400(%1)\n\t"
            "vmovaps %%ymm0, 384(%1)\n\t"

	    "vmovaps 512(%0), %%ymm0\n\t"
            "vmovaps 528(%0), %%ymm1\n\t"
            "vmovaps 544(%0), %%ymm2\n\t"
            "vmovaps 560(%0), %%ymm3\n\t"
            "vmovaps %%ymm3, 560(%1)\n\t"
            "vmovaps %%ymm2, 544(%1)\n\t"
            "vmovaps %%ymm1, 528(%1)\n\t"
            "vmovaps %%ymm0, 512(%1)\n\t"

	    "vmovaps 640(%0), %%ymm0\n\t"
            "vmovaps 656(%0), %%ymm1\n\t"
            "vmovaps 672(%0), %%ymm2\n\t"
            "vmovaps 688(%0), %%ymm3\n\t"
            "vmovaps %%ymm3, 688(%1)\n\t"
            "vmovaps %%ymm2, 672(%1)\n\t"
            "vmovaps %%ymm1, 656(%1)\n\t"
            "vmovaps %%ymm0, 640(%1)\n\t"

	    "vmovaps 640(%0), %%ymm0\n\t"
            "vmovaps 656(%0), %%ymm1\n\t"
            "vmovaps 672(%0), %%ymm2\n\t"
            "vmovaps 688(%0), %%ymm3\n\t"
            "vmovaps %%ymm3, 688(%1)\n\t"
            "vmovaps %%ymm2, 672(%1)\n\t"
            "vmovaps %%ymm1, 656(%1)\n\t"
            "vmovaps %%ymm0, 640(%1)\n\t"

	    : "=r" (dst)
	    : "r" (src)
	    : "ymm0", "ymm1", "ymm2", "ymm3");

	*/
//	memcpy(dst, src, offsetof(struct position, memcpy_offset));

	invariant(value_bounds(dst->material_value[white]));
	invariant(value_bounds(dst->material_value[black]));

	adjust_move_counters(dst, m);
	clear_from_square(dst, m.from);
	if (is_capture(m))
		clear_captured(dst, src, m);
	set_to_square(dst, m.to, m.result);
	adjust_castle_rights(dst, m);
	if (is_pawn_move(m) || m.captured == pawn) {
		generate_pawn_reach_maps(dst);
	}
	else if (move_eq(m, mcastle_white_king_side())) {
		clear_from_square(dst, h1);
		set_to_square(dst, f1, rook);
	}
	else if (move_eq(m, mcastle_white_queen_side())) {
		clear_from_square(dst, a1);
		set_to_square(dst, d1, rook);
	}
	else if (move_eq(m, mcastle_black_king_side())) {
		clear_from_square(dst, h8);
		set_to_square(dst, f8, rook);
	}
	else if (move_eq(m, mcastle_black_queen_side())) {
		clear_from_square(dst, a8);
		set_to_square(dst, d8, rook);
	}

	dst->white_ki = bsf(dst->map[white_king]);
	dst->black_ki = bsf(dst->map[black_king]);

	pos_switch_turn(dst);
	setup_en_passant_index(dst, m);
	accumulate_occupancy(dst);
	update_all_rays(dst, m);
	generate_white_attacks(dst);
	generate_black_attacks(dst);
	search_king_attacks(dst);
	search_pins(dst);
	accumulate_misc_patterns(dst, white);
	accumulate_misc_patterns(dst, black);
	find_hanging_pieces(dst);
}

int
position_flip(struct position *restrict dst,
	      const struct position *restrict src)
{
	assert(!is_in_check(src));
	assert(!position_has_en_passant_target(src));

	struct position_desc desc;
	memset(&desc, 0, sizeof(desc));

	for (int i = 0; i < 64; ++i) {
		if (pos_piece_at(src, i) != 0) {
			desc.board[flip_i(i)] =
			    opponent_of(pos_player_at(src, i));
			desc.board[flip_i(i)] |= pos_piece_at(src, i);
		}
	}

	desc.castle_rights[cri_white_king_side] = src->cr_black_king_side;
	desc.castle_rights[cri_white_queen_side] = src->cr_black_queen_side;
	desc.castle_rights[cri_black_king_side] = src->cr_white_king_side;
	desc.castle_rights[cri_black_queen_side] = src->cr_white_queen_side;

	desc.turn = src->turn;
	desc.en_passant_index = 0;
	desc.half_move_counter = src->half_move_counter;
	desc.full_move_counter = src->full_move_counter;

	return position_reset(dst, desc);
}
