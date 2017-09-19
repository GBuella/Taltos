
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <stdlib.h>
#include <string.h>

#include "position.h"
#include "constants.h"
#include "hash.h"
#include "util.h"
#include "eval.h"
#include "flip_chess_board.h"
#include "flip_board_pairs.h"


/* Little getter functions, used in bookkeeping - non performance critical */

bool
pos_is_check(const struct position *pos)
{
	return is_in_check(pos);
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

	if (a->ep_index != b->ep_index)
		return false;
	if (a->cr_king_side != b->cr_king_side)
		return false;
	if (a->cr_queen_side != b->cr_queen_side)
		return false;
	if (a->cr_opponent_king_side != b->cr_opponent_king_side)
		return false;
	if (a->cr_opponent_queen_side != b->cr_opponent_queen_side)
		return false;

	return true;
}

int
position_square_at(const struct position *pos, int index)
{
	return pos_square_at(pos, index);
}

enum piece
position_piece_at(const struct position *pos, int index)
{
	return pos_piece_at(pos, index);
}

enum player
position_player_at(const struct position *pos, int index)
{
	return pos_player_at(pos, index);
}

bool
position_has_en_passant_index(const struct position *pos)
{
	return pos_has_ep_target(pos);
}

int
position_get_en_passant_index(const struct position *pos)
{
	return pos->ep_index;
}

bool
position_cr_king_side(const struct position *pos)
{
	return pos->cr_king_side;
}

bool
position_cr_queen_side(const struct position *pos)
{
	return pos->cr_queen_side;
}

bool
position_cr_opponent_king_side(const struct position *pos)
{
	return pos->cr_opponent_king_side;
}

bool
position_cr_opponent_queen_side(const struct position *pos)
{
	return pos->cr_opponent_queen_side;
}

void
get_position_key(const struct position *pos, uint64_t key[static 2])
{
	key[0] = pos->zhash[0];
	key[1] = pos->zhash[1];
}

static bool
has_potential_ep_captor(struct position *pos, int index)
{
	return ((ind_file(index) != file_h)
	    && is_nonempty(pos->map[pawn] & bit64(index + EAST)))
	    ||
	    ((ind_file(index) != file_a)
	    && is_nonempty(pos->map[pawn] & bit64(index + WEST)));
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
static void update_all_rays(struct position*, move);
static void generate_player_attacks(struct position*);
static void generate_opponent_attacks(struct position*);
static void search_king_attacks(struct position*);

static void
generate_attack_maps(struct position *pos)
{
	// Generating the bitboards in the pos->rays array.
	generate_all_rays(pos);

	// Generating the bitboards in the pos->attack array.
	generate_player_attacks(pos);
	generate_opponent_attacks(pos);

	/*
	 * For a king in check, generate a bitboard of attackers.
	 * While looking for sliding attacks on the king, also
	 * generate bitboards of pins. The four routines below only turn on
	 * bits in the maps, there they must cleared first.
	 */
	pos->king_attack_map = EMPTY;

	search_king_attacks(pos);
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

	generate_rays(pos, pos->rays[pr_hor], WEST, FILE_A);
	generate_rays(pos, pos->rays[pr_hor], EAST, FILE_H);
	generate_rays(pos, pos->rays[pr_ver], NORTH, RANK_8);
	generate_rays(pos, pos->rays[pr_ver], SOUTH, RANK_1);
	generate_rays(pos, pos->rays[pr_diag], SOUTH + EAST, RANK_1 | FILE_H);
	generate_rays(pos, pos->rays[pr_diag], NORTH + WEST, RANK_8 | FILE_A);
	generate_rays(pos, pos->rays[pr_adiag], SOUTH + WEST, RANK_1 | FILE_A);
	generate_rays(pos, pos->rays[pr_adiag], NORTH + EAST, RANK_8 | FILE_H);
}

static void
update_rays_empty_square(uint64_t rays[64], int i)
{
	uint64_t r = rays[i];
	for (uint64_t map = r; is_nonempty(map); map = reset_lsb(map))
		rays[bsf(map)] |= r & ~lsb(map);
}

static void
update_all_rays_empty_square(struct position *pos, int i)
{
	update_rays_empty_square(pos->rays[pr_hor], i);
	update_rays_empty_square(pos->rays[pr_ver], i);
	update_rays_empty_square(pos->rays[pr_diag], i);
	update_rays_empty_square(pos->rays[pr_adiag], i);
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
	update_rays_occupied_square(pos->rays[pr_hor], i);
	update_rays_occupied_square(pos->rays[pr_ver], i);
	update_rays_occupied_square(pos->rays[pr_diag], i);
	update_rays_occupied_square(pos->rays[pr_adiag], i);
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

	for (; is_nonempty(bishops); bishops = reset_lsb(bishops)) {
		accumulator |= pos->rays[pr_diag][bsf(bishops)];
		accumulator |= pos->rays[pr_adiag][bsf(bishops)];
	}

	return accumulator;
}

static uint64_t
rook_attacks(uint64_t rooks, const struct position *pos)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(rooks); rooks = reset_lsb(rooks)) {
		accumulator |= pos->rays[pr_hor][bsf(rooks)];
		accumulator |= pos->rays[pr_ver][bsf(rooks)];
	}

	return accumulator;
}

static uint64_t
knight_attacks(uint64_t knights)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(knights); knights = reset_lsb(knights))
		accumulator |= knight_pattern(bsf(knights));

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
	pos->attack[king] = king_moves_table[pos->ki];
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
	pos->attack[opponent_king] = king_moves_table[pos->opp_ki];
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
add_ranged_king_attacker(struct position *pos, uint64_t bandit)
{
	assert(is_singular(bandit));
	pos->king_attack_map |= bandit;
	pos->king_attack_map |= ray_table[pos->ki][bsf(bandit)];
}

static void
search_king_attacks(struct position *pos)
{
	pos->king_attack_map =
	    pawn_attacks_player(pos->map[king]) & pos->map[opponent_pawn];

	pos->king_attack_map |=
	    knight_pattern(pos->ki) & pos->map[opponent_knight];

	uint64_t bandit = pos->rays[pr_diag][pos->ki] & pos->bq[1];
	if (is_nonempty(bandit))
		add_ranged_king_attacker(pos, bandit);

	bandit = pos->rays[pr_adiag][pos->ki] & pos->bq[1];
	if (is_nonempty(bandit))
		add_ranged_king_attacker(pos, bandit);

	bandit = pos->rays[pr_hor][pos->ki] & pos->rq[1];
	if (is_nonempty(bandit))
		add_ranged_king_attacker(pos, bandit);

	bandit = pos->rays[pr_ver][pos->ki] & pos->rq[1];
	if (is_nonempty(bandit))
		add_ranged_king_attacker(pos, bandit);
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
static int board_reset(struct position*, const char board[static 64]);

// Check for too many pawns, pawns on wrong rank
static bool pawns_valid(const struct position*);

// Check if the given square has pawn residing on it, which was possibly just
// recently pushed there with a double push
static bool can_be_valid_ep_index(const struct position*, int index);

// Checks placements of kings and rooks being consistent with castling rights
static bool castle_rights_valid(const struct position*);

// Checks for valid placement - and existence - of the two kings
static bool kings_valid(const struct position*);

static void accumulate_occupancy(struct position*);

int
position_reset(struct position *pos,
		const char board[static 64],
		const bool castle_rights[static 4],
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
	if (board_reset(pos, board) != 0)
		return -1;
	accumulate_occupancy(pos);
	pos->cr_king_side = castle_rights[cri_king_side];
	pos->cr_queen_side = castle_rights[cri_queen_side];
	pos->cr_opponent_king_side = castle_rights[cri_opponent_king_side];
	pos->cr_opponent_queen_side = castle_rights[cri_opponent_queen_side];
	pos->ep_index = ep_index;
	if (ep_index != 0 && !can_be_valid_ep_index(pos, ep_index))
		return -1;
	if (popcnt(pos->map[0]) > 16)
		return -1;
	if (popcnt(pos->map[1]) > 16)
		return -1;
	if (!kings_valid(pos))
		return -1;
	pos->ki = bsf(pos->map[king]);
	pos->opp_ki = bsf(pos->map[opponent_king]);
	if (!pawns_valid(pos))
		return -1;
	if (!castle_rights_valid(pos))
		return -1;
	generate_attack_maps(pos);
	generate_pawn_reach_maps(pos);
	generate_opponent_pawn_reach_maps(pos);
	setup_zhash(pos);
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

static bool
kings_valid(const struct position *pos)
{
	// Are there exactly one of each king?
	if (!is_singular(pos->map[king]))
		return false;
	if (!is_singular(pos->map[opponent_king]))
		return false;

	// Are the two kings too close?
	int k0 = bsf(pos->map[king]);
	uint64_t k1 = pos->map[opponent_king];
	if (is_nonempty(king_moves_table[k0] & k1))
		return false;

	return true;
}

static bool
pawns_valid(const struct position *pos)
{
	uint64_t pawns = pos->map[pawn];
	uint64_t opponent_pawns = pos->map[opponent_pawn];

	// Each side can have up to 8 pawns
	if (popcnt(pawns) > 8)
		return false;
	if (popcnt(opponent_pawns) > 8)
		return false;

	// No pawn can reside on rank #1 or #8
	if (is_nonempty((pawns | opponent_pawns) & (RANK_1 | RANK_8)))
		return false;

	return true;
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
	pos->rq[0] = pos->map[rook] | pos->map[queen];
	pos->bq[0] = pos->map[bishop] | pos->map[queen];
	pos->rq[1] = pos->map[opponent_rook] | pos->map[opponent_queen];
	pos->bq[1] = pos->map[opponent_bishop] | pos->map[opponent_queen];
	pos->map[0] = pos->map[pawn] | pos->map[knight] | pos->map[king];
	pos->map[0] |= pos->rq[0] | pos->bq[0];
	pos->map[1] = pos->map[opponent_pawn];
	pos->map[1] |= pos->map[opponent_knight];
	pos->map[1] |= pos->map[opponent_king];
	pos->map[1] |= pos->rq[1] | pos->bq[1];

	pos->occupied = pos->map[0] | pos->map[1];
}

static int
board_reset(struct position *pos, const char board[static 64])
{
	for (int i = 0; i < 64; ++i) {
		if (board[i] != 0) {
			if (!is_valid_piece(board[i] & ~1))
				return -1;
			pos_add_piece(pos, i, board[i]);
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



#ifdef TALTOS_CAN_USE_INTEL_AVX2

static void
flip_ray_array(uint64_t dst[restrict 64], const uint64_t src[restrict 64])
{
	const __m256i key = _mm256_setr_epi8( 7,  6,  5,  4,  3,  2,  1,  0,
	                                     15, 14, 13, 12, 11, 10,  9,  8,
	                                      7,  6,  5,  4,  3,  2,  1,  0,
	                                     15, 14, 13, 12, 11, 10,  9,  8);


	int rs = 0;
	int rd = 48;
	do {
		const __m256i *restrict src32 = (const __m256i*)(src + rs);
		__m256i *restrict dst32 = (__m256i*)(dst + rd);

		__m256i vector0 = _mm256_load_si256(src32 + 2);
		__m256i vector1 = _mm256_load_si256(src32 + 3);
		__m256i vector2 = _mm256_load_si256(src32 + 0);
		__m256i vector3 = _mm256_load_si256(src32 + 1);
		dst32[0] = _mm256_shuffle_epi8(vector0, key);
		dst32[1] = _mm256_shuffle_epi8(vector1, key);
		dst32[2] = _mm256_shuffle_epi8(vector2, key);
		dst32[3] = _mm256_shuffle_epi8(vector3, key);

		//dst32[0] = _mm256_shuffle_epi8(src32[0], key);
		//dst32[1] = _mm256_shuffle_epi8(src32[1], key);

		rs += 16;
		rd -= 16;
	} while (rs < 64);

}

#elif defined(TALTOS_CAN_USE_INTEL_AVX) && 0
// XXX it actually is SSSE3

static void
flip_ray_array(uint64_t dst[restrict 64], const uint64_t src[restrict 64])
{
	int rs = 0;
	int rd = 56;
	do {
		const __m128i *restrict src16 = (const __m128i*)(src + rs);
		__m128i *restrict dst16 = (__m128i*)(dst + rd);

		dst16[0] = _mm_shuffle_epi8(src16[0], ray_flip_shufflekey_16());
		dst16[1] = _mm_shuffle_epi8(src16[1], ray_flip_shufflekey_16());
		dst16[2] = _mm_shuffle_epi8(src16[2], ray_flip_shufflekey_16());
		dst16[3] = _mm_shuffle_epi8(src16[3], ray_flip_shufflekey_16());

		rs += 8;
		rd -= 8;
	} while (rs < 64);
}

#else

static inline void
flip_ray_array(uint64_t dst[restrict 64] attribute(align_value(pos_alignment)),
		const uint64_t src[restrict 64] attribute(align_value(pos_alignment)))
{
	int rs = 0;
	int rd = 56;
	do {
		dst[rd + 0] = bswap(src[rs + 0]);
		dst[rd + 1] = bswap(src[rs + 1]);
		dst[rd + 2] = bswap(src[rs + 2]);
		dst[rd + 3] = bswap(src[rs + 3]);
		dst[rd + 4] = bswap(src[rs + 4]);
		dst[rd + 5] = bswap(src[rs + 5]);
		dst[rd + 6] = bswap(src[rs + 6]);
		dst[rd + 7] = bswap(src[rs + 7]);
		rs += 8;
		rd -= 8;
	} while (rs < 64);
}

#endif

static void
generate_reach_table(struct position *pos, uint8_t table[64],
    			int dir, uint8_t edge)
{
	for (int from = 0; from < 64; ++from) {
		table[from] = from;

		uint64_t bit = bit64(from);
		if (is_nonempty(edge & bit))
			continue;

		uint64_t stop = edge | pos->occupied;

		do {
			table[from] += dir;
			bit = bit64(table[from]);
		} while (is_empty(stop & bit));
	}
}

static void
generate_all_reach_tables(struct position *pos)
{
	memset(pos->reach, 0, sizeof(pos->reach));

	generate_reach_table(pos, pos->reach[r_west], WEST, FILE_A);
	generate_reach_table(pos, pos->reach[r_east], EAST, FILE_H);
	generate_reach_table(pos, pos->reach[r_north], NORTH, RANK_8);
	generate_reach_table(pos, pos->reach[r_south], SOUTH, RANK_1);
	generate_reach_table(pos, pos->reach[r_south_east],
	    SOUTH + EAST, RANK_1 | FILE_H);
	generate_reach_table(pos, pos->reach[r_north_west],
	    NORTH + WEST, RANK_8 | FILE_A);
	generate_reach_table(pos, pos->reach[r_south_west],
	    SOUTH + WEST, RANK_1 | FILE_A);
	generate_reach_table(pos, pos->reach[r_north_east],
	    NORTH + EAST, RANK_8 | FILE_H);
}


static void
make_move_all_reach_tables(struct position *restrict dst,
			const struct position *restrict src,
			move m)
{
	make_move_reach_table(dst->reach[r_east],
	    src->reach[r_east], src->reach[r_west], m);
	make_move_reach_table(dst->reach[r_west],
	    src->reach[r_west], src->reach[r_east], m);
	make_move_reach_table(dst->reach[r_north],
	    src->reach[r_south], src->reach[r_north], m);
	make_move_reach_table(dst->reach[r_south],
	    src->reach[r_north], src->reach[r_south], m);
	make_move_reach_table(dst->reach[r_south_east],
	    src->reach[r_north_east], src->reach[r_south_west], m);
	make_move_reach_table(dst->reach[r_south_west],
	    src->reach[r_north_west], src->reach[r_south_east], m);
	make_move_reach_table(dst->reach[r_north_east],
	    src->reach[r_south_east], src->reach[r_north_west], m);
	make_move_reach_table(dst->reach[r_north_west],
	    src->reach[r_south_west], src->reach[r_north_east], m);
}

static void
flip_all_rays(struct position *restrict dst, const struct position *restrict src)
{
	flip_ray_array(dst->rays[pr_hor], src->rays[pr_hor]);
	flip_ray_array(dst->rays[pr_ver], src->rays[pr_ver]);
	flip_ray_array(dst->rays[pr_diag], src->rays[pr_adiag]);
	flip_ray_array(dst->rays[pr_adiag], src->rays[pr_diag]);
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
	pos->ep_index = 0;

#endif
}

void
position_flip(struct position *restrict dst,
		const struct position *restrict src)
{
	assert(!is_in_check(src));

	flip_chess_board(dst->board, src->board);
	dst->attack[0] = bswap(src->attack[1]);
	dst->attack[1] = bswap(src->attack[0]);
	flip_2_bb_pairs(dst->attack + 2, src->attack + 2);
	flip_2_bb_pairs(dst->attack + 6, src->attack + 6);
	flip_2_bb_pairs(dst->attack + 10, src->attack + 10);
	flip_2_bb_pairs(dst->sliding_attacks, src->sliding_attacks);
	flip_piece_maps(dst, src);
	dst->occupied = dst->map[0] | dst->map[1];
	flip_all_rays(dst, src);
	flip_tail(dst, src);
	clear_extra_bitboards(dst);
	dst->ki = bsf(dst->map[king]);
	dst->opp_ki = bsf(dst->map[opponent_king]);
	dst->occupied = bswap(src->occupied);
	dst->king_attack_map = EMPTY;
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

bool
is_move_irreversible(const struct position *pos, move m)
{
	return (mtype(m) != mt_general)
	    || (mcapturedp(m) != 0)
	    || (mfrom(m) == sq_a1 && pos->cr_queen_side)
	    || (mfrom(m) == sq_h1 && pos->cr_king_side)
	    || (mfrom(m) == sq_e1 && (pos->cr_queen_side || pos->cr_king_side))
	    || (mto(m) == sq_a8 && pos->cr_opponent_queen_side)
	    || (mto(m) == sq_h8 && pos->cr_opponent_king_side)
	    || (pos->board[mfrom(m)] == pawn);
}

bool
has_any_legal_move(const struct position *pos)
{
	move moves[MOVE_ARRAY_LENGTH];
	return gen_moves(pos, moves) != 0;
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
	if (mtype(m) == mt_castle_kingside) {
		pos->board[sq_e8] = 0;
		pos->board[sq_g8] = king;
		pos->board[sq_f8] = rook;
		pos->board[sq_h8] = 0;
		pos->map[opponent_rook] ^= SQ_F8 | SQ_H8;
		pos->map[opponent_king] ^= SQ_E8 | SQ_G8;
	}
	else if (mtype(m) == mt_castle_queenside) {
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

		if (mtype(m) == mt_promotion)
			pos->opponent_material_value +=
			    piece_value[mresultp(m)] - pawn_value;

		invariant(value_bounds(pos->material_value));
		enum piece porig = pos_piece_at(pos, mfrom(m));
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
	generate_player_attacks(dst);
	generate_opponent_attacks(dst);
	z2_xor_move(dst->zhash, m);
}

void
position_make_move(struct position *restrict dst,
			const struct position *restrict src,
			move m)
{
	// this wrapper function is pointless now
	make_move(dst, src, m);
}
