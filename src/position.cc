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

#include "position.h"
#include "transposition_table.h"
#include "util.h"
#include "eval.h"
#include "move_desc.h"
#include "flip_chess_board.h"
#include "flip_board_pairs.h"
#include "flip_ray_array.h"

#include "bitboard_constants.inc"

using std::array;

namespace taltos
{

bool position::is_draw_with_insufficient_material() const
{
	// Look at all pieces except kings.
	bitboard pieces = occupied & ~(map[king] | map[opponent_king]);

	// Are there only kings and bishops left?
	// This also covers the case where nothing but kings are left.
	if (pieces == (map[bishop] | map[opponent_bishop])) {
		// All bishops on the same color?
		if (is_empty(pieces & black_squares))
			return true;
		if (is_empty(pieces & white_squares))
			return true;
	}
	else if (pieces.is_singular() and board[pieces.ls1b_index()] == knight) {
		return true; // Only a single knight left
	}

	return false;
}

bool positon::operator ==(const position& other) const;
{
	for (unsigned i = 0; i < board.size(); ++i) {
		if (board[i] != other.board[i])
			return false;
	}

	if (ep_index != other.ep_index)
		return false;
	if (cr_king_side != other.cr_king_side)
		return false;
	if (cr_queen_side != other.cr_queen_side)
		return false;
	if (cr_opponent_king_side != other.cr_opponent_king_side)
		return false;
	if (cr_opponent_queen_side != other.cr_opponent_queen_side)
		return false;

	return true;
}

static bool
has_potential_ep_captor(const position *pos, int index)
{
	return ((ind_file(index) != file_h)
	    and is_nonempty(pos->pawns() & bb(index + east)))
	    or
	    ((ind_file(index) != file_a)
	    and is_nonempty(pos->pawns() & bb(index + west)));
}


/*
 * Setup the attack bitboards.
 * One bitboard per each piece type/side, to represent
 * the squares attacked by thath piece.
 * Opponents sliding pieces are special cases, their attacks
 * maps are calculated as if they could jump over the king of the side to move.
 * This makes it easier to find squares where a king in check can move to.
 */
static void generate_all_rook_rays(array<bitboard, 64>& rays, bitboard occupied)
static void generate_all_bishop_rays(array<bitboard, 64>& rays, bitboard occupied)
static void update_all_rays(struct position*, move);
static void generate_player_attacks(struct position*);
static void generate_opponent_attacks(struct position*);
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

static void
search_pins(struct position *pos)
{
	search_player_king_pins(pos, 0);
	search_player_king_pins(pos, 1);
}

static void
generate_rays(array<bitboard, 64>& rays, int dir, bitboard edge, bitboard occupied)
{
	for (int from = 0; from < 64; ++from) {
		bitboards bit = bb(from);
		if (is_nonempty(edge & bit))
			continue;

		bitboard stop = edge | occupied;
		int i = from;

		do {
			i += dir;
			bit = bb(i);
			rays[from] |= bit;
		} while (is_empty(stop & bit));
	}
}

static void
generate_all_rook_rays(array<bitboard, 64>& rays, bitboard occupied)
{
	rays.fill(0);

	generate_rays(rays, west, file_a, occupied);
	generate_rays(rays, east, file_h, occupied);
	generate_rays(rays, north, rank_8, occupied);
	generate_rays(rays, south, rank_1, occupied);
}

static void
generate_all_bishop_rays(array<bitboard, 64>& rays, bitboard occupied)
{
	rays.fill(0);

	generate_rays(rays, south + east, rank_1 | file_h, occupied);
	generate_rays(rays, north + west, rank_8 | file_a, occupied);
	generate_rays(rays, south + west, rank_1 | file_a, occupied);
	generate_rays(rays, north + east, rank_8 | file_h, occupied);
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
update_all_rays(struct position *pos, move m)
{
	update_all_rays_empty_square(pos, mfrom(m));
	if (mtype(m) == mt_en_passant) {
		update_all_rays_occupied_square(pos, mto(m));
		update_all_rays_empty_square(pos, mto(m) + NORTH);
	}
	else if (!is_capture(m)) {
		update_all_rays_occupied_square(pos, mto(m));
		if (mtype(m) == mt_castle_kingside) {
			update_all_rays_occupied_square(pos, sq_f8);
		}
		else if (mtype(m) == mt_castle_queenside) {
			update_all_rays_occupied_square(pos, sq_d8);
		}
	}
}

static uint64_t
bishop_attacks(uint64_t bishops, const struct position *pos)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(bishops); bishops = reset_lsb(bishops))
		accumulator |= pos->rays[pr_bishop][bsf(bishops)];

	return accumulator;
}

static uint64_t
rook_attacks(uint64_t rooks, const struct position *pos)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(rooks); rooks = reset_lsb(rooks))
		accumulator |= pos->rays[pr_rook][bsf(rooks)];

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

static void
generate_player_attacks(struct position *pos)
{
	pos->attack[pawn] = pawn_attacks_player(pos->map[pawn]);
	pos->attack[king] = king_pattern[pos->ki];
	pos->attack[knight] = knight_attacks(pos->map[knight]);
	pos->attack[bishop] = bishop_attacks(pos->map[bishop], pos);
	pos->attack[rook] = rook_attacks(pos->map[rook], pos);
	pos->attack[queen] = bishop_attacks(pos->map[queen], pos);
	pos->attack[queen] |= rook_attacks(pos->map[queen], pos);
	accumulate_attacks(pos->attack, pos->sliding_attacks);
}

static void
generate_opponent_attacks(struct position *pos)
{
	pos->attack[opponent_pawn] =
	    pawn_attacks_opponent(pos->map[opponent_pawn]);
	pos->attack[opponent_king] = king_pattern[pos->opp_ki];
	pos->attack[opponent_knight] =
	    knight_attacks(pos->map[opponent_knight]);
	pos->attack[opponent_bishop] =
	    bishop_attacks(pos->map[opponent_bishop], pos);
	pos->attack[opponent_rook] =
	    rook_attacks(pos->map[opponent_rook], pos);
	pos->attack[opponent_queen] =
	    bishop_attacks(pos->map[opponent_queen], pos);
	pos->attack[opponent_queen] |=
	    rook_attacks(pos->map[opponent_queen], pos);

	accumulate_attacks(pos->attack + 1, pos->sliding_attacks + 1);
}

static void
search_king_attacks(struct position *pos)
{
	pos->king_danger_map = EMPTY;

	pos->king_attack_map =
	    pawn_attacks_player(pos->map[king]) & pos->map[opponent_pawn];

	pos->king_attack_map |=
	    knight_pattern[pos->ki] & pos->map[opponent_knight];

	uint64_t breach = bishop_reach(pos, pos->ki);
	uint64_t bandits = breach & pos->bq[1];
	for (; is_nonempty(bandits); bandits = reset_lsb(bandits)) {
		uint64_t bandit = lsb(bandits);
		uint64_t reach = breach;

		if (is_nonempty(bandit & diag_masks[pos->ki]))
			reach &= diag_masks[pos->ki];
		else
			reach &= adiag_masks[pos->ki];

		pos->king_danger_map |= reach & ~bandit;

		reach &= bishop_reach(pos, bsf(bandit));
		pos->king_attack_map |= reach | bandit;
	}

	uint64_t rreach = rook_reach(pos, pos->ki);
	bandits = rreach & pos->rq[1];
	for (; is_nonempty(bandits); bandits = reset_lsb(bandits)) {
		uint64_t bandit = lsb(bandits);
		uint64_t reach = rreach;

		if (is_nonempty(bandit & hor_masks[pos->ki]))
			reach &= hor_masks[pos->ki];
		else
			reach &= ver_masks[pos->ki];

		pos->king_danger_map |= reach & ~bandit;

		reach &= rook_reach(pos, bsf(bandit));
		pos->king_attack_map |= reach | bandit;
	}
}


// Generate the pawn_attack_reach, and half_open_files bitboards

static void
generate_pawn_reach_maps(struct position *pos)
{
	uint64_t reach = kogge_stone_north(north_of(pos->map[pawn]));

	pos->half_open_files[0] = ~kogge_stone_south(reach);
	pos->pawn_attack_reach[0] = (west_of(reach) & ~FILE_H) |
	    (east_of(reach) & ~FILE_A);
}

static void
generate_opponent_pawn_reach_maps(struct position *pos)
{
	uint64_t reach = kogge_stone_south(south_of(pos->map[opponent_pawn]));

	pos->half_open_files[1] = ~kogge_stone_north(reach);
	pos->pawn_attack_reach[1] = (west_of(reach) & ~FILE_H) |
	    (east_of(reach) & ~FILE_A);
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
static int board_reset(struct position*, const array<int, 64>& input_board);

// Check for too many pawns, pawns on wrong rank
static void check_pawns(const struct position*);

// Check if the given square has pawn residing on it, which was possibly just
// recently pushed there with a double push
static bool can_be_valid_ep_index(const struct position*, int index);

// Checks placements of kings and rooks being consistent with castling rights
static bool castle_rights_valid(const struct position*);

// Checks for valid placement - and existence - of the two kings
static void check_kings(const struct position*);

static void check_king_attacks(const struct position*);

static void accumulate_occupancy(struct position*);

static void accumulate_misc_patterns(struct position*, int player);

void
position_reset(struct position *pos,
	       const std::array<int, 64>& input_board,
	       const std::array<bool, 4>& castle_rights,
	       int ep_index)
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
	board_reset(pos, input_board);
	accumulate_occupancy(pos);
	pos->cr_king_side = castle_rights[cri_king_side];
	pos->cr_queen_side = castle_rights[cri_queen_side];
	pos->cr_opponent_king_side = castle_rights[cri_opponent_king_side];
	pos->cr_opponent_queen_side = castle_rights[cri_opponent_queen_side];
	pos->ep_index = ep_index;
	if (ep_index != 0 && !can_be_valid_ep_index(pos, ep_index))
		throw string("Invalid en passant square");
	if (popcnt(pos->map[0]) > 16)
		throw string("Invalid number of pieces");
	if (popcnt(pos->map[1]) > 16)
		throw string("Invalid number of pieces");
	check_kings(pos);
	pos->ki = bsf(pos->map[king]);
	pos->opp_ki = bsf(pos->map[opponent_king]);
	check_pawns(pos);
	if (!castle_rights_valid(pos))
		throw string("Invalid castling rights");
	generate_all_rook_rays(rook_rays, occupied);
	generate_all_bishop_rays(bishop_rays, occupied);
	generate_player_attacks(pos);
	generate_opponent_attacks(pos);
	generate_pawn_reach_maps(pos);
	generate_opponent_pawn_reach_maps(pos);
	check_king_attacks(pos);
	search_king_attacks(pos);
	search_pins(pos);
	accumulate_misc_patterns(pos, 0);
	accumulate_misc_patterns(pos, 1);
	setup_zhash(pos);
	find_hanging_pieces(pos);
}

struct position*
position_allocate(void)
{
	struct position *pos = xaligned_alloc(pos_alignment, sizeof *pos);
	memset(pos, 0, sizeof *pos);
	return pos;
}

struct position*
position_create(const char board[static 64],
		const bool castle_rights[static 4],
		int en_passant_index)
{
	struct position *pos = xaligned_alloc(pos_alignment, sizeof *pos);
	if (position_reset(pos, board, castle_rights, en_passant_index) == 0) {
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
	pos->zhash[0] = 0;
	pos->zhash[1] = 0;
	for (uint64_t occ = pos->occupied;
	    is_nonempty(occ);
	    occ = reset_lsb(occ)) {
		int i = bsf(occ);
		z2_toggle_sq(pos->zhash, i,
		    pos_piece_at(pos, i),
		    pos_player_at(pos, i));
	}
	if (pos->cr_king_side)
		z2_toggle_castle_king_side(pos->zhash);
	if (pos->cr_queen_side)
		z2_toggle_castle_queen_side(pos->zhash);
	if (pos->cr_opponent_king_side)
		z2_toggle_castle_king_side_opponent(pos->zhash);
	if (pos->cr_opponent_queen_side)
		z2_toggle_castle_queen_side_opponent(pos->zhash);
}

static void
check_kings(const struct position *pos)
{
	// Are there exactly one of each king?
	if (is_empty(pos->map[king]))
		throw string("Missing king");
	if (is_empty(pos->map[opponent_king]))
		throw string("Missing king");
	if (!is_singular(pos->map[king]))
		throw string("Too many kings");
	if (!is_singular(pos->map[opponent_king]))
		throw string("Too many kings");

	// Are the two kings too close?
	int k0 = bsf(pos->map[king]);
	uint64_t k1 = pos->map[opponent_king];
	if (is_nonempty(king_pattern[k0] & k1))
		throw string("Invalid king placement");
}

static void
check_king_attacks(const struct position *pos)
{
	if (is_nonempty(pos->attack[0] & pos->map[opponent_king]))
		throw string("Invalid king placement");

	uint64_t k = pos->map[king];

	if (popcnt(pawn_attacks_player(k) & pos->map[opponent_pawn]) > 1)
		throw string("Invalid king placement");

	if (popcnt(pos->rays[pr_bishop][pos->ki] & pos->bq[1]) > 1)
		throw string("Invalid king placement");

	uint64_t rq_attackers = pos->rays[pr_rook][pos->ki] & pos->rq[1];

	if (popcnt(rq_attackers) > 2)
		throw string("Invalid king placement");

	if (popcnt(rq_attackers & ~RANK_1) > 1)
		throw string("Invalid king placement");

	if (popcnt(knight_pattern[pos->ki] & pos->map[opponent_knight]) > 1)
		throw string("Invalid king placement");
}

static void
check_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[pawn];
	uint64_t opponent_pawns = pos->map[opponent_pawn];

	// Each side can have up to 8 pawns
	if (popcnt(pawns) > 8)
		throw string("Too many pawns");
	if (popcnt(opponent_pawns) > 8)
		throw string("Too many pawns");

	// No pawn can reside on rank #1 or #8
	if (is_nonempty((pawns | opponent_pawns) & (RANK_1 | RANK_8)))
		throw string("Invalid pawn placement");
}

static bool
castle_rights_valid(const struct position *pos)
{
	if (pos->cr_king_side) {
		if (pos_square_at(pos, sq_e1) != king
		    || pos_square_at(pos, sq_h1) != rook)
			return false;
	}
	if (pos->cr_queen_side) {
		if (pos_square_at(pos, sq_e1) != king
		    || pos_square_at(pos, sq_a1) != rook)
			return false;
	}
	if (pos->cr_opponent_king_side) {
		if (pos_square_at(pos, sq_e8) != opponent_king
		    || pos_square_at(pos, sq_h8) != opponent_rook)
			return false;
	}
	if (pos->cr_opponent_queen_side) {
		if (pos_square_at(pos, sq_e8) != opponent_king
		    || pos_square_at(pos, sq_a8) != opponent_rook)
			return false;
	}
	return true;
}

static void
pos_add_piece(struct position *pos, int i, int piece)
{
	extern const int piece_value[PIECE_ARRAY_SIZE];

	invariant(ivalid(i));
	pos->board[i] = piece & ~1;
	pos->occupied |= bit64(i);
	pos->map[piece & 1] |= bit64(i);
	pos->map[piece] |= bit64(i);

	if ((piece & ~1) != king) {
		if ((piece & 1) == 0)
			pos->material_value += piece_value[piece & ~1];
		else
			pos->opponent_material_value += piece_value[piece & ~1];
	}
}

static void
accumulate_occupancy(struct position *pos)
{
	pos->nb[0] = pos->map[knight] | pos->map[bishop];
	pos->nb[1] = pos->map[opponent_knight] | pos->map[opponent_bishop];
	pos->rq[0] = pos->map[rook] | pos->map[queen];
	pos->rq[1] = pos->map[opponent_rook] | pos->map[opponent_queen];
	pos->all_rq = pos->rq[0] | pos->rq[1];
	pos->bq[0] = pos->map[bishop] | pos->map[queen];
	pos->bq[1] = pos->map[opponent_bishop] | pos->map[opponent_queen];
	pos->all_bq = pos->bq[0] | pos->bq[1];
	pos->map[0] = pos->map[pawn] | pos->map[knight] | pos->map[king];
	pos->map[0] |= pos->rq[0] | pos->bq[0];
	pos->map[1] = pos->map[opponent_pawn];
	pos->map[1] |= pos->map[opponent_knight];
	pos->map[1] |= pos->map[opponent_king];
	pos->map[1] |= pos->rq[1] | pos->bq[1];
	pos->all_kings = pos->map[king] | pos->map[opponent_king];
	pos->all_knights = pos->map[knight] | pos->map[opponent_knight];
	pos->occupied = pos->map[0] | pos->map[1];
}

static void
accumulate_misc_patterns(struct position *pos, int player)
{
	pos->undefended[player] = pos->map[player] & ~pos->attack[player];
}

static int
board_reset(struct position *pos, const array<int, 64>& input_board);
{
	for (int i = 0; i < 64; ++i) {
		if (board[i] != 0) {
			if (!is_valid_piece(input_board[i] & ~1))
				throw string("Invalid piece");
			pos_add_piece(pos, i, input_board[i]);
		}
	}
	return 0;
}

static bool
can_be_valid_ep_index(const struct position *pos, int index)
{
	uint64_t bit = bit64(index);

	if (ind_rank(index) != rank_5)
		return false;
	if (is_empty(pos->map[opponent_pawn] & bit))
		return false;
	if (is_nonempty(pos->occupied & north_of(bit)))
		return false;
	if (is_nonempty(pos->occupied & north_of(north_of(bit))))
		return false;

	return true;
}



/*
 * attack[2] should be 32 byte aligned, thus attack[0] must be 16 bytes away
 * from being 32 byte aligned. The same is true of map[2].
 */
static_assert(offsetof(struct position, attack) % 32 == 16, "alignment error");

static void
flip_piece_maps(struct position *restrict dst,
		const struct position *restrict src)
{
	flip_2_bb_pairs(dst->map + 2, src->map + 2);
	flip_2_bb_pairs(dst->map + 6, src->map + 6);
	flip_2_bb_pairs(dst->map + 10, src->map + 10);
	flip_2_bb_pairs(dst->half_open_files, src->half_open_files);
}



static void
flip_all_rays(struct position *restrict dst, const struct position *restrict src)
{
	flip_ray_array(dst->rays[pr_bishop], src->rays[pr_bishop]);
	flip_ray_array(dst->rays[pr_rook], src->rays[pr_rook]);
}



/*
 * flip_tail
 * Swapping two pairs of 64 bit values at the end of struct position.
 *
 * uint64_t zhash[2]
 *
 * 64 bits:
 * int8_t cr_king_side;
 * int8_t cr_queen_side;
 * int8_t cr_padding0[2];
 * int32_t material_value;
 *
 * 64 bits describing the same from the opponent's point of view:
 * int8_t cr_opponent_king_side;
 * int8_t cr_opponent_queen_side;
 * int8_t cr_padding1[2];
 * int32_t opponent_material_value;
 *
 */

#ifdef TALTOS_CAN_USE_INTEL_AVX

static_assert(offsetof(struct position, zhash) % 32 == 0, "alignment error");

static void
flip_tail(struct position *restrict dst,
	const struct position *restrict src)
{
	__m256d *dst32 = (__m256d*)(dst->zhash);
	const __m256d *src32 = (const __m256d*)(src->zhash);

	*dst32 = _mm256_permute_pd(*src32, 1 + (1 << 2));
}

#elif defined(TALTOS_CAN_USE_INTEL_SHUFFLE_EPI32)

static_assert(offsetof(struct position, zhash) % 16 == 0, "alignment error");

static void
flip_tail(struct position *restrict dst,
	const struct position *restrict src)
{
	const __m128i attribute(align_value(16)) *restrict src16
	    = (const __m128i*)(src->zhash);
	__m128i attribute(align_value(16)) *restrict dst16
	    = (__m128i*)(dst->zhash);

	dst16[0] = _mm_shuffle_epi32(src16[0], (1 << 6) | (3 << 2) | 2);
	dst16[1] = _mm_shuffle_epi32(src16[1], (1 << 6) | (3 << 2) | 2);
}

#else

static void
flip_tail(struct position *restrict dst,
	const struct position *restrict src)
{
	dst->zhash[0] = src->zhash[1];
	dst->zhash[1] = src->zhash[0];
	dst->cr_king_side = src->cr_opponent_king_side;
	dst->cr_queen_side = src->cr_opponent_queen_side;
	dst->material_value = src->opponent_material_value;
	dst->cr_opponent_king_side = src->cr_king_side;
	dst->cr_opponent_queen_side = src->cr_queen_side;
	dst->opponent_material_value = src->material_value;
}

#endif


static void
adjust_castle_rights_move(struct position *pos, move m)
{
	if (pos->cr_king_side && mto(m) == sq_h1) {
		z2_toggle_castle_king_side(pos->zhash);
		pos->cr_king_side = false;
	}
	if (pos->cr_queen_side && mto(m) == sq_a1) {
		z2_toggle_castle_queen_side(pos->zhash);
		pos->cr_queen_side = false;
	}
	if (pos->cr_opponent_king_side) {
		if (mfrom(m) == sq_h8 || mfrom(m) == sq_e8) {
			z2_toggle_castle_king_side_opponent(pos->zhash);
			pos->cr_opponent_king_side = false;
		}
	}
	if (pos->cr_opponent_queen_side) {
		if (mfrom(m) == sq_a8 || mfrom(m) == sq_e8) {
			z2_toggle_castle_queen_side_opponent(pos->zhash);
			pos->cr_opponent_queen_side = false;
		}
	}
}




static void
clear_extra_bitboards(struct position *pos)
{
#ifdef HAS_YMM_ZERO

	__m256i *addr = (void*)&pos->king_attack_map;
	*addr = ymm_zero;

#else

	pos->king_attack_map = EMPTY;
	pos->king_danger_map = EMPTY;
	pos->ep_index = 0;

#endif
}

void
position_flip(struct position *restrict dst,
		const struct position *restrict src)
{
	assert(!is_in_check(src));

	flip_chess_board(dst->board, src->board);
	clear_extra_bitboards(dst);
	dst->occupied = bswap(src->occupied);
	dst->ki = flip_i(src->opp_ki);
	dst->opp_ki = flip_i(src->ki);
	dst->attack[0] = bswap(src->attack[1]);
	dst->attack[1] = bswap(src->attack[0]);
	flip_2_bb_pairs(dst->attack + 2, src->attack + 2);
	flip_2_bb_pairs(dst->attack + 6, src->attack + 6);
	flip_2_bb_pairs(dst->attack + 10, src->attack + 10);
	flip_2_bb_pairs(dst->sliding_attacks, src->sliding_attacks);
	flip_piece_maps(dst, src);
	flip_2_bb_pairs(dst->rq, src->rq);
	flip_all_rays(dst, src);
	flip_tail(dst, src);
	flip_2_bb_pairs(dst->king_pins, src->king_pins);
	dst->undefended[0] = bswap(src->undefended[1]);
	dst->undefended[1] = bswap(src->undefended[0]);
	dst->all_kings = bswap(src->all_kings);
	dst->all_knights = bswap(src->all_knights);
	dst->all_rq = bswap(src->all_rq);
	dst->all_bq = bswap(src->all_bq);
	flip_chess_board(dst->hanging, src->hanging);
	dst->hanging_map = bswap(src->hanging_map);
}

bool
is_legal_move(const struct position *pos, move m)
{
	move moves[MOVE_ARRAY_LENGTH];

	(void) gen_moves(pos, moves);
	for (unsigned i = 0; moves[i] != 0; ++i) {
		if (moves[i] == m)
			return true;
	}
	return false;
}

bool position::is_move_irreversible(move m) const
{
	return (m.type != mt_general)
	    or (m.captured() != 0)
	    or (m.from == a1 and cr_queen_side)
	    or (m.from == h1 and cr_king_side)
	    or (m.from == e1 and (cr_queen_side or cr_king_side))
	    or (m.to == a8 and cr_opponent_queen_side)
	    or (m.to == h8 and cr_opponent_king_side)
	    or (board[m.from] == pawn);
}

static void
clear_to_square(struct position *pos, int i)
{
	if (pos->board[i] != 0) {
		pos->material_value -= piece_value[(unsigned)(pos->board[i])];
		invariant(pos->material_value >= 0);
		invariant(value_bounds(pos->material_value));
		pos->map[(unsigned char)(pos->board[i])] &= ~bit64(i);
	}
}

static void
move_pawn(struct position *pos, move m)
{
	pos->board[mto(m)] = pawn;
	pos->board[mfrom(m)] = 0;
	pos->map[opponent_pawn] ^= m64(m);
	if (mtype(m) == mt_en_passant) {
		pos->board[mto(m) + NORTH] = 0;
		pos->map[pawn] &= ~bit64(mto(m) + NORTH);
		pos->material_value -= pawn_value;
		invariant(pos->material_value >= 0);
		invariant(value_bounds(pos->material_value));

	}
	else if (mtype(m) == mt_pawn_double_push) {
		if (has_potential_ep_captor(pos, mto(m)))
			pos->ep_index = mto(m);
	}
}

static void
move_piece(struct position *pos, move m)
{
	if (m == flip_m(mcastle_king_side)) {
		pos->board[sq_e8] = 0;
		pos->board[sq_g8] = king;
		pos->board[sq_f8] = rook;
		pos->board[sq_h8] = 0;
		pos->map[opponent_rook] ^= SQ_F8 | SQ_H8;
		pos->map[opponent_king] ^= SQ_E8 | SQ_G8;
	}
	else if (m == flip_m(mcastle_queen_side)) {
		pos->board[sq_e8] = 0;
		pos->board[sq_c8] = king;
		pos->board[sq_d8] = rook;
		pos->board[sq_a8] = 0;
		pos->map[opponent_rook] ^= SQ_D8 | SQ_A8;
		pos->map[opponent_king] ^= SQ_E8 | SQ_C8;
	}
	else {
		uint64_t from = mfrom64(m);
		uint64_t to = mto64(m);
		enum piece porig = pos_piece_at(pos, mfrom(m));

		pos->opponent_material_value +=
			piece_value[mresultp(m)] - piece_value[porig];

		invariant(value_bounds(pos->material_value));

		pos->map[porig + 1] &= ~from;
		pos->map[mresultp(m) + 1] |= to;
		pos->board[mfrom(m)] = 0;
		pos->board[mto(m)] = mresultp(m);
	}
}

void
make_move(struct position *restrict dst,
		const struct position *restrict src,
		move m)
{
	m = flip_m(m);
	flip_chess_board(dst->board, src->board);
	flip_piece_maps(dst, src);
	clear_extra_bitboards(dst);
	flip_all_rays(dst, src);
	flip_tail(dst, src);

	update_all_rays(dst, m);

	invariant(value_bounds(dst->material_value));
	invariant(value_bounds(dst->opponent_material_value));
	clear_to_square(dst, mto(m));
	dst->ki = bsf(dst->map[king]);
	if (mresultp(m) == pawn) {
		dst->opp_ki = bsf(dst->map[opponent_king]);
		move_pawn(dst, m);
		generate_opponent_pawn_reach_maps(dst);
	}
	else {
		move_piece(dst, m);
		dst->opp_ki = bsf(dst->map[opponent_king]);
		adjust_castle_rights_move(dst, m);
		if (is_promotion(m))
			generate_opponent_pawn_reach_maps(dst);
	}
	if (mcapturedp(m) == pawn)
		generate_pawn_reach_maps(dst);
	accumulate_occupancy(dst);
	search_king_attacks(dst);
	search_pins(dst);
	generate_player_attacks(dst);
	generate_opponent_attacks(dst);
	accumulate_misc_patterns(dst, 0);
	accumulate_misc_patterns(dst, 1);
	z2_xor_move(dst->zhash, m);
	find_hanging_pieces(dst);
}

void
position_make_move(struct position *restrict dst,
			const struct position *restrict src,
			move m)
{
	// this wrapper function is pointless now
	make_move(dst, src, m);
}

void*
position::operator new(size_t size)
{
	return taltos::alloc_align64(size);
}

void
position::operator delete(void* pointer)
{
	free_align64(pointer);
}

void*
position::operator new[](size_t size)
{
	return taltos::alloc_align64(size);
}

void
position::operator delete[](void* pointer)
{
	free_align64(pointer);
}

}
