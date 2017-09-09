
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <stdlib.h>
#include <string.h>

#include "position.h"
#include "constants.h"
#include "hash.h"
#include "util.h"
#include "eval.h"


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
 *
 * The routine generate_attack_maps generates the attack maps from scratch,
 * while generate_attacks_move updates attack maps during make_move.
 */
static void generate_player_attacks(struct position*, int player);
static void search_pawn_king_attacks(struct position*);
static void search_knight_king_attacks(struct position*);
static void search_bishop_king_attacks(struct position*);
static void search_rook_king_attacks(struct position*);
static void generate_all_rays(struct position*);
static void update_all_rays(struct position*, move);

static void
generate_attack_maps(struct position *pos)
{
	generate_all_rays(pos);

	// Generating the bitboards in the pos->attack array.
	generate_player_attacks(pos, 0);
	generate_player_attacks(pos, 1);

	/*
	 * For a king in check, generate a bitboard of attackers.
	 * While looking for sliding attacks on the king, also
	 * generate bitboards of pins. The four routines below only turn on
	 * bits in the maps, there they must cleared first.
	 */
	pos->king_attack_map = EMPTY;
	pos->king_danger_map = EMPTY;

	search_pawn_king_attacks(pos);
	search_knight_king_attacks(pos);
	search_bishop_king_attacks(pos);
	search_rook_king_attacks(pos);
	pos->king_danger_map |= pos->attack[1];
}

static void
generate_rays(uint64_t bb[static 64], uint64_t occ,
				int dir, uint64_t edge)
{
	for (int i = 0; i < 64; ++i) {
		int bit_i = i;
		uint64_t bit = bit64(bit_i);
		while ((bit & edge) == EMPTY) {
			bit_i += dir;
			bit = bit64(bit_i);
			bb[i] |= bit;
			if ((bit & occ) != EMPTY)
				break;
		}
	}
}

static void
generate_all_rays(struct position *pos)
{
	generate_rays(pos->rays[pr_south], pos->occupied,
	    SOUTH, RANK_1);
	generate_rays(pos->rays[pr_north], pos->occupied,
	    NORTH, RANK_8);
	generate_rays(pos->rays[pr_west], pos->occupied,
	    WEST, FILE_A);
	generate_rays(pos->rays[pr_east], pos->occupied,
	    EAST, FILE_H);
	generate_rays(pos->rays[pr_south_east], pos->occupied,
	    SOUTH + EAST, FILE_H | RANK_1);
	generate_rays(pos->rays[pr_south_west], pos->occupied,
	    SOUTH + WEST, FILE_A | RANK_1);
	generate_rays(pos->rays[pr_north_east], pos->occupied,
	    NORTH + EAST, FILE_H | RANK_8);
	generate_rays(pos->rays[pr_north_west], pos->occupied,
	    NORTH + WEST, FILE_A | RANK_8);
	for (int i = 0; i < 64; ++i) {
		pos->rays_bishops[i] =
		    pos->rays[pr_north_east][i] | pos->rays[pr_north_west][i] |
		    pos->rays[pr_south_east][i] | pos->rays[pr_south_west][i];
		pos->rays_rooks[i] =
		    pos->rays[pr_north][i] | pos->rays[pr_south][i] |
		    pos->rays[pr_east][i] | pos->rays[pr_west][i];
	}
}

static void
generate_attacks_move(struct position *restrict dst,
			const struct position *restrict src,
			move m)
{
	uint64_t move_map = dst->occupied ^ bswap(src->occupied);

	if (is_capture(m) || is_nonempty(move_map & dst->sliding_attacks[0]))
		generate_player_attacks(dst, 0);
	else
		dst->attack[0] = bswap(src->attack[1]);

	generate_player_attacks(dst, 1);
}

static uint64_t
bishop_attacks(uint64_t bishops, const struct position *pos)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(bishops); bishops = reset_lsb(bishops))
		accumulator |= pos->rays_bishops[bsf(bishops)];

	return accumulator;
}

static uint64_t
rook_attacks(uint64_t rooks, const struct position *pos)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(rooks); rooks = reset_lsb(rooks))
		accumulator |= pos->rays_rooks[bsf(rooks)];

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
generate_player_attacks(struct position *pos, int player)
{
	uint64_t *attack = pos->attack + player;
	uint64_t *sliding_attacks = pos->sliding_attacks + player;
	const uint64_t *map = pos->map + player;

	if (player == 0)
		attack[pawn] = pawn_attacks_player(map[pawn]);
	else
		attack[pawn] = pawn_attacks_opponent(map[pawn]);
	attack[king] = king_moves_table[bsf(map[king])];
	attack[knight] = knight_attacks(map[knight]);
	attack[bishop] = bishop_attacks(map[bishop], pos);
	attack[rook] = rook_attacks(map[rook], pos);
	attack[queen] = bishop_attacks(map[queen], pos);
	attack[queen] |= rook_attacks(map[queen], pos);
	*sliding_attacks = attack[bishop];
	*sliding_attacks |= attack[rook];
	*sliding_attacks |= attack[queen];
	attack[0] = *sliding_attacks;
	attack[0] |= attack[pawn];
	attack[0] |= attack[knight];
	attack[0] |= attack[king];
}

static void
search_king_sliding_attack(struct position *pos, int rayi, uint64_t bandits)
{
	uint64_t ray = pos->rays[rayi][pos->king_index];
	uint64_t bandit = ray & bandits;
	if (is_empty(bandit))
		return;

	pos->king_attack_map |= ray;
	pos->king_danger_map |= ray & ~bandit;
	ray = pos->rays[opposite_ray_index(rayi)][pos->king_index];
	pos->king_danger_map |= ray;
}

static void
search_bishop_king_attacks(struct position *pos)
{
	uint64_t bishops = pos->map[opponent_bishop] | pos->map[opponent_queen];

	search_king_sliding_attack(pos, pr_south_east, bishops);
	search_king_sliding_attack(pos, pr_north_east, bishops);
	search_king_sliding_attack(pos, pr_south_west, bishops);
	search_king_sliding_attack(pos, pr_north_west, bishops);
}

static void
search_rook_king_attacks(struct position *pos)
{
	uint64_t rooks = pos->map[opponent_rook] | pos->map[opponent_queen];

	search_king_sliding_attack(pos, pr_south, rooks);
	search_king_sliding_attack(pos, pr_north, rooks);
	search_king_sliding_attack(pos, pr_west, rooks);
	search_king_sliding_attack(pos, pr_east, rooks);
}

static void
search_knight_king_attacks(struct position *pos)
{
	pos->king_attack_map |=
	    knight_pattern(pos->king_index) & pos->map[opponent_knight];
}

static void
search_pawn_king_attacks(struct position *pos)
{
	pos->king_attack_map |=
	    pawn_attacks_player(pos->map[king]) & pos->map[opponent_pawn];
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
	pos->king_index = bsf(pos->map[king]);
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



/* BEGIN CSTYLED */
#if defined(TALTOS_CAN_USE_INTEL_AVX2)

static __m256i
flip_shufflekey_32(void)
{
	return _mm256_setr_epi8(15, 14, 13, 12, 11, 10,  9,  8,
	                         7,  6,  5,  4,  3,  2,  1,  0,
	                        15, 14, 13, 12, 11, 10,  9,  8,
	                         7,  6,  5,  4,  3,  2,  1,  0);
}

static __m256i
ray_flip_shufflekey_32(void)
{
	return _mm256_setr_epi8( 7,  6,  5,  4,  3,  2,  1,  0,
	                        15, 14, 13, 12, 11, 10,  9,  8,
	                         7,  6,  5,  4,  3,  2,  1,  0,
	                        15, 14, 13, 12, 11, 10,  9,  8);
}


#elif defined(TALTOS_CAN_USE_INTEL_SHUFFLE_EPI8)

static __m128i
flip_shufflekey_16(void)
{
    return _mm_setr_epi8(15, 14, 13, 12, 11, 10,  9,  8,
	                  7,  6,  5,  4,  3,  2,  1,  0);
}

#endif
/* END CSTYLED */



/*
 * flip_board
 *
 * Flip the board, i.e. exchange rank #1 with rank #8,
 * rank #2 with rank #7 etc..
 *
 * A rank here uses 8 bytes, thus 64 bit values
 * are shuffled around, while in flip_piece_maps
 * a rank is stored in 8 bits, so chars are shuffled around.
 */
#if defined(TALTOS_CAN_USE_INTEL_AVX2)

#define FLIP_BOARD_SELECTOR (0 + (1 << 4) + (2 << 2) + (3 << 0))

static void
flip_board(struct position *restrict dst,
		const struct position *restrict src)
{
	const __m256d *restrict src32 = (const __m256d*)(src->board);
	__m256d *restrict dst32 = (__m256d*)(dst->board);

	dst32[1] = _mm256_permute4x64_pd(src32[0], FLIP_BOARD_SELECTOR);
	dst32[0] = _mm256_permute4x64_pd(src32[1], FLIP_BOARD_SELECTOR);
}

#undef FLIP_BOARD_SELECTOR

#elif defined(TALTOS_CAN_USE_INTEL_AVX)

static void
flip_board(struct position *restrict dst,
		const struct position *restrict src)
{
	__m256d temp;
	const __m256d attribute(align_value(32)) *restrict src32 =
	    (const __m256d*)(src->board);
	__m256d attribute(align_value(32)) *restrict dst32 =
	    (__m256d*)(dst->board);

	temp = _mm256_permute_pd(src32[0], 1 + (1 << 2));
	dst32[1] = _mm256_permute2f128_pd(temp, temp, 1);
	temp = _mm256_permute_pd(src32[1], 1 + (1 << 2));
	dst32[0] = _mm256_permute2f128_pd(temp, temp, 1);
}

#else

static void
flip_board(struct position *restrict dst,
		const struct position *restrict src)
{
	for (int i = 0; i < 64; i += 8)
		memcpy(dst->board + (56 - i), src->board + i, 8);
}

#endif



/*
 * flip_piece_maps
 *
 * Flipping piece map bitboard pairs upside down
 * i.e. reversing the order of octets in each 16 octets
 *
 * player's bitboard:     .1....1.       ...1....
 *                        ..1..1..       ...1....
 *                        ...11...       ...1....
 *                        ...11...       ...1....
 *                        ..1..1..  -->  ...1....
 *                        .1....1.       ..1.1...
 *                        1......1       .1...1..
 *                        ........       1.....1.
 *
 * opponent's bitboard:   1.....1.       ........
 *                        .1...1..       1......1
 *                        ..1.1...       .1....1.
 *                        ...1....       ..1..1..
 *                        ...1....  -->  ...11...
 *                        ...1....       ...11...
 *                        ...1....       ..1..1..
 *                        ...1....       .1....1.
 *
 * The following members of struct position are handled here:
 *
 * uint64_t attack[2];
 * .
 * .
 * .
 * uint64_t attack[13];
 * uint64_t sliding_attacks[0];
 * uint64_t sliding_attacks[1];
 * uint64_t map[0];
 * .
 * .
 * .
 * uint64_t map[13];
 *
 * This should amount to 32 bitboards, 14 pairs of bitboards.
 * Each pair is a 128 bit value, that can be handle with some SSE
 * instructions.
 * Alternatively, each 256 wide quadruplet ( 7 of them ) of bitboards
 * can be handle by a few AVX / AVX2 instructions.
 *
 * Note, thath attack[0] and attack[1] are not treated here.
 */

#define FLIP_COUNT 32

// make sure these follow each other
static_assert(offsetof(struct position, sliding_attacks) ==
	offsetof(struct position, attack) + PIECE_ARRAY_SIZE * sizeof(uint64_t),
	"Invalid struct position");

static_assert(offsetof(struct position, map) ==
	offsetof(struct position, sliding_attacks) + 2 * sizeof(uint64_t),
	"Invalid struct position");

// This is expected to consist of FLIP_COUNT bitboards
static_assert(
	offsetof(struct position, pawn_attack_reach) +
		sizeof(((struct position*)NULL)->pawn_attack_reach) -

	// exclude 2 bitboards: attack[0] and attack[1]
	offsetof(struct position, attack) - 2 * sizeof(uint64_t)

	==

	FLIP_COUNT * sizeof(uint64_t),
	"Invalid struct position");

#ifdef TALTOS_CAN_USE_INTEL_AVX2
/*
 * attack[2] should be 32 byte aligned, thus attack[0] must be 16 bytes away
 * from being 32 byte aligned.
 */
static_assert(offsetof(struct position, attack) % 32 == 16, "alignment error");

static void
flip_piece_maps(struct position *restrict dst,
		const struct position *restrict src)
{
	const __m256i *restrict src32 = (const __m256i*)(src->attack + 2);
	__m256i *restrict dst32 = (__m256i*)(dst->attack + 2);

	for (int i = 0; i < (FLIP_COUNT / 4); ++i)
		dst32[i] = _mm256_shuffle_epi8(src32[i], flip_shufflekey_32());
}

#elif defined(TALTOS_CAN_USE_INTEL_SHUFFLE_EPI8)

static_assert(offsetof(struct position, attack) % 16 == 0, "alignment error");

static void
flip_piece_maps(struct position *restrict dst,
		const struct position *restrict src)
{
	const __m128i attribute(align_value(16)) *restrict src16
	    = (const __m128i*)(src->attack + 2);
	__m128i attribute(align_value(16)) *restrict dst16
	    = (__m128i*)(dst->attack + 2);

	for (int i = 0; i < (FLIP_COUNT / 2); ++i)
		dst16[i] = _mm_shuffle_epi8(src16[i], flip_shufflekey_16());
}

#else

static void
flip_piece_maps(struct position *restrict dst,
		const struct position *restrict src)
{
	for (int i = 2; i < PIECE_ARRAY_SIZE; i += 2) {
		dst->attack[i] = bswap(src->attack[i + 1]);
		dst->attack[i + 1] = bswap(src->attack[i]);
	}
	dst->sliding_attacks[0] = bswap(src->sliding_attacks[1]);
	dst->sliding_attacks[1] = bswap(src->sliding_attacks[0]);
	for (int i = 0; i < PIECE_ARRAY_SIZE; i += 2) {
		dst->map[i] = bswap(src->map[i + 1]);
		dst->map[i + 1] = bswap(src->map[i]);
	}
	dst->half_open_files[0] = bswap(src->half_open_files[1]);
	dst->half_open_files[1] = bswap(src->half_open_files[0]);
	dst->pawn_attack_reach[0] = bswap(src->pawn_attack_reach[1]);
	dst->pawn_attack_reach[1] = bswap(src->pawn_attack_reach[0]);
}

#endif


/*
 * flip_rays
 */

#ifdef TALTOS_CAN_USE_INTEL_AVX2

static void
flip_ray_array(uint64_t dst[restrict 64], const uint64_t src[restrict 64])
{
	int rs = 0;
	int rd = 56;
	do {
		const __m256i *restrict src32 = (const __m256i*)(src + rs);
		__m256i *restrict dst32 = (__m256i*)(dst + rd);

		dst32[0] = _mm256_shuffle_epi8(src32[0],
		    ray_flip_shufflekey_32());
		dst32[1] = _mm256_shuffle_epi8(src32[1],
		    ray_flip_shufflekey_32());

		rs += 8;
		rd -= 8;
	} while (rs < 64);
}

#else

static void
flip_ray_array(uint64_t dst[restrict 64], const uint64_t src[restrict 64])
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
flip_rays(struct position *restrict dst, const struct position *restrict src)
{
	flip_ray_array(dst->rays[pr_north], src->rays[pr_south]);
	flip_ray_array(dst->rays[pr_south], src->rays[pr_north]);
	flip_ray_array(dst->rays[pr_west], src->rays[pr_west]);
	flip_ray_array(dst->rays[pr_east], src->rays[pr_east]);
	flip_ray_array(dst->rays[pr_north_east], src->rays[pr_south_east]);
	flip_ray_array(dst->rays[pr_north_west], src->rays[pr_south_west]);
	flip_ray_array(dst->rays[pr_south_east], src->rays[pr_north_east]);
	flip_ray_array(dst->rays[pr_south_west], src->rays[pr_north_west]);
	flip_ray_array(dst->rays_bishops, src->rays_bishops);
	flip_ray_array(dst->rays_rooks, src->rays_rooks);
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
	assert(!pos_has_ep_target(src));

	flip_board(dst, src);
	dst->attack[0] = bswap(src->attack[1]);
	dst->attack[1] = bswap(src->attack[0]);
	flip_piece_maps(dst, src);
	flip_rays(dst, src);
	flip_tail(dst, src);
	dst->king_attack_map = EMPTY;
	dst->king_danger_map = dst->attack[1];
	dst->ep_index = 0;
	dst->king_index = bsf(dst->map[king]);
	dst->occupied = bswap(src->occupied);
}

static void
append_rays(uint64_t bb[64], uint64_t merged[64], uint64_t which,
		uint64_t append)
{
	while (is_nonempty(which)) {
		bb[bsf(which)] |= append;
		merged[bsf(which)] |= append;
		which = reset_lsb(which);
	}
}

static void
block_rays(uint64_t bb[64], uint64_t merged[64], uint64_t which, uint64_t block)
{
	uint64_t mask = ~block;
	while (is_nonempty(which)) {
		bb[bsf(which)] &= mask;
		merged[bsf(which)] &= mask;
		which = reset_lsb(which);
	}
}

static void
update_rays_empty_square(struct position *pos, int i)
{
	append_rays(pos->rays[pr_west], pos->rays_rooks,
	    pos->rays[pr_east][i], pos->rays[pr_west][i]);
	append_rays(pos->rays[pr_east], pos->rays_rooks,
	    pos->rays[pr_west][i], pos->rays[pr_east][i]);
	append_rays(pos->rays[pr_north], pos->rays_rooks,
	    pos->rays[pr_south][i], pos->rays[pr_north][i]);
	append_rays(pos->rays[pr_south], pos->rays_rooks,
	    pos->rays[pr_north][i], pos->rays[pr_south][i]);
	append_rays(pos->rays[pr_north_east], pos->rays_bishops,
	    pos->rays[pr_south_west][i], pos->rays[pr_north_east][i]);
	append_rays(pos->rays[pr_south_west], pos->rays_bishops,
	    pos->rays[pr_north_east][i], pos->rays[pr_south_west][i]);
	append_rays(pos->rays[pr_north_west], pos->rays_bishops,
	    pos->rays[pr_south_east][i], pos->rays[pr_north_west][i]);
	append_rays(pos->rays[pr_south_east], pos->rays_bishops,
	    pos->rays[pr_north_west][i], pos->rays[pr_south_east][i]);
}

static void
update_rays_occupied_square(struct position *pos, int i)
{
	block_rays(pos->rays[pr_west], pos->rays_rooks,
	    pos->rays[pr_east][i], pos->rays[pr_west][i]);
	block_rays(pos->rays[pr_east], pos->rays_rooks,
	    pos->rays[pr_west][i], pos->rays[pr_east][i]);
	block_rays(pos->rays[pr_north], pos->rays_rooks,
	    pos->rays[pr_south][i], pos->rays[pr_north][i]);
	block_rays(pos->rays[pr_south], pos->rays_rooks,
	    pos->rays[pr_north][i], pos->rays[pr_south][i]);
	block_rays(pos->rays[pr_north_east], pos->rays_bishops,
	    pos->rays[pr_south_west][i], pos->rays[pr_north_east][i]);
	block_rays(pos->rays[pr_south_west], pos->rays_bishops,
	    pos->rays[pr_north_east][i], pos->rays[pr_south_west][i]);
	block_rays(pos->rays[pr_north_west], pos->rays_bishops,
	    pos->rays[pr_south_east][i], pos->rays[pr_north_west][i]);
	block_rays(pos->rays[pr_south_east], pos->rays_bishops,
	    pos->rays[pr_north_west][i], pos->rays[pr_south_east][i]);
}

static void
update_all_rays(struct position *pos, move m)
{
	update_rays_empty_square(pos, mfrom(m));
	if (mtype(m) == mt_en_passant) {
		update_rays_occupied_square(pos, mto(m));
		update_rays_empty_square(pos, mto(m) + NORTH);
	}
	else if (!is_capture(m)) {
		update_rays_occupied_square(pos, mto(m));
		if (mtype(m) == mt_castle_kingside) {
			update_rays_occupied_square(pos, sq_f8);
		}
		else if (mtype(m) == mt_castle_queenside) {
			update_rays_occupied_square(pos, sq_d8);
		}
	}
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
		pos->map[0] &= ~bit64(i);
	}
}

static void
move_pawn(struct position *pos, move m)
{
	pos->board[mto(m)] = pawn;
	pos->board[mfrom(m)] = 0;
	pos->map[opponent_pawn] ^= m64(m);
	pos->map[1] ^= m64(m);
	if (mtype(m) == mt_en_passant) {
		pos->board[mto(m) + NORTH] = 0;
		pos->map[pawn] &= ~bit64(mto(m) + NORTH);
		pos->map[0] &= ~bit64(mto(m) + NORTH);
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
		pos->map[1] ^= SQ_H8 | SQ_F8 | SQ_E8 | SQ_G8;
	}
	else if (mtype(m) == mt_castle_queenside) {
		pos->board[sq_e8] = 0;
		pos->board[sq_c8] = king;
		pos->board[sq_d8] = rook;
		pos->board[sq_a8] = 0;
		pos->map[opponent_rook] ^= SQ_D8 | SQ_A8;
		pos->map[opponent_king] ^= SQ_E8 | SQ_C8;
		pos->map[1] ^= SQ_D8 | SQ_A8 | SQ_C8 | SQ_E8;
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
		pos->map[1] &= ~from;
		pos->map[1] |= to;
	}
}

void
make_move(struct position *restrict dst,
		const struct position *restrict src,
		move m)
{
	m = flip_m(m);
	flip_board(dst, src);
	flip_piece_maps(dst, src);
	clear_extra_bitboards(dst);
	flip_rays(dst, src);
	flip_tail(dst, src);
	update_all_rays(dst, m);

	invariant(value_bounds(dst->material_value));
	invariant(value_bounds(dst->opponent_material_value));
	clear_to_square(dst, mto(m));
	if (mresultp(m) == pawn) {
		dst->king_index = bsf(dst->map[king]);
		move_pawn(dst, m);
		search_pawn_king_attacks(dst);
		generate_opponent_pawn_reach_maps(dst);
	}
	else {
		move_piece(dst, m);
		adjust_castle_rights_move(dst, m);
		dst->king_index = bsf(dst->map[king]);
		search_knight_king_attacks(dst);
		if (is_promotion(m))
			generate_opponent_pawn_reach_maps(dst);
	}
	if (mcapturedp(m) == pawn)
		generate_pawn_reach_maps(dst);
	dst->occupied = dst->map[0] | dst->map[1];
	if (move_gives_check(m)) {
		search_bishop_king_attacks(dst);
		search_rook_king_attacks(dst);
	}
	generate_attacks_move(dst, src, m);
	dst->king_danger_map |= dst->attack[1];
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
