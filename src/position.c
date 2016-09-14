
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <stdlib.h>
#include <string.h>

#include "position.h"
#include "constants.h"
#include "hash.h"
#include "util.h"
#include "vector_values.h"
#include "eval.h"

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

static uint64_t
knight_attacks(uint64_t knights)
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(knights); knights = reset_lsb(knights))
		accumulator |= knight_pattern(bsf(knights));

	return accumulator;
}

static uint64_t
knight_attacks_nonempty(uint64_t knights)
{
	uint64_t accumulator = EMPTY;

	do {
		accumulator |= knight_pattern(bsf(knights));
	} while (is_nonempty(knights = reset_lsb(knights)));

	return accumulator;
}

static uint64_t
generate_sliding_attacks(uint64_t pieces, uint64_t occupied,
				struct magical magic[static const 64])
{
	uint64_t accumulator = EMPTY;

	for (; is_nonempty(pieces); pieces = reset_lsb(pieces))
		accumulator |= sliding_map(occupied, magic + bsf(pieces));

	return accumulator;
}

static uint64_t
bishop_attacks(uint64_t bishops, uint64_t occupied)
{
	return generate_sliding_attacks(bishops, occupied, bishop_magics);
}

static uint64_t
rook_attacks(uint64_t rooks, uint64_t occupied)
{
	return generate_sliding_attacks(rooks, occupied, rook_magics);
}

static uint64_t
generate_sliding_attacks_nonempty(uint64_t pieces, uint64_t occupied,
					struct magical magic[static const 64])
{
	uint64_t accumulator = EMPTY;

	do {
		accumulator |= sliding_map(occupied, magic + bsf(pieces));
	} while (is_nonempty(pieces = reset_lsb(pieces)));

	return accumulator;
}

static uint64_t
queen_attacks_nonempty(uint64_t queens, uint64_t occupied)
{
	uint64_t accumulator = EMPTY;

	do {
		accumulator |= sliding_map(occupied, rook_magics + bsf(queens))
		    | sliding_map(occupied, bishop_magics + bsf(queens));
	} while (is_nonempty(queens = reset_lsb(queens)));

	return accumulator;
}

static uint64_t
bishop_attacks_nonempty(uint64_t bishops, uint64_t occupied)
{
	return
	    generate_sliding_attacks_nonempty(bishops, occupied, bishop_magics);
}

static uint64_t
rook_attacks_nonempty(uint64_t rooks, uint64_t occupied)
{
	return generate_sliding_attacks_nonempty(rooks, occupied, rook_magics);
}

static void
generate_player_attacks(struct position *pos)
{
	uint64_t all_attacks;

	all_attacks = pos->attack[pawn] =
	    pawn_attacks_player(pos->map[pawn]);
	all_attacks |= pos->attack[king] =
	    king_moves_table[bsf(pos->map[king])];
	all_attacks |= pos->attack[knight] =
	    knight_attacks(pos->map[knight]);
	pos->sliding_attacks[0] = pos->attack[bishop] =
	    bishop_attacks(pos->map[bishop], pos->occupied);
	pos->sliding_attacks[0] |= pos->attack[rook] =
	    rook_attacks(pos->map[rook], pos->occupied);
	pos->sliding_attacks[0] |= pos->attack[queen] =
	    bishop_attacks(pos->map[queen], pos->occupied)
	    | rook_attacks(pos->map[queen], pos->occupied);
	all_attacks |= pos->sliding_attacks[0];
	pos->attack[0] = all_attacks;
}

static void
generate_opponent_attacks(struct position *pos)
{
	uint64_t all_attacks;

	all_attacks = pos->attack[opponent_pawn] =
	    pawn_attacks_opponent(pos->map[opponent_pawn]);
	all_attacks |= pos->attack[opponent_king] =
	    king_moves_table[bsf(pos->map[opponent_king])];
	all_attacks |= pos->attack[opponent_knight] =
	    knight_attacks(pos->map[opponent_knight]);
	uint64_t occupied = pos->occupied & ~pos->map[king];
	pos->sliding_attacks[1] = pos->attack[opponent_bishop] =
	    bishop_attacks(pos->map[opponent_bishop], occupied);
	pos->sliding_attacks[1] |= pos->attack[opponent_rook] =
	    rook_attacks(pos->map[opponent_rook], occupied);
	pos->sliding_attacks[1] |= pos->attack[opponent_queen] =
	    bishop_attacks(pos->map[opponent_queen], occupied)
	    | rook_attacks(pos->map[opponent_queen], occupied);
	all_attacks |= pos->sliding_attacks[1];
	pos->attack[1] = all_attacks;
}

static void
search_bishop_king_attacks(struct position *pos)
{
	uint64_t bishops = pos->map[opponent_bishop] | pos->map[opponent_queen];
	uint64_t pinnable_pieces = pos->map[0];
	if (pos_has_ep_target(pos))
		pinnable_pieces |= bit64(pos->ep_index);

	bishops &= bishop_pattern_table[pos->king_index];
	for (; is_nonempty(bishops); bishops = reset_lsb(bishops)) {
		uint64_t ray = ray_table[pos->king_index][bsf(bishops)];
		uint64_t ray_occ = ray & pos->occupied;

		if (is_empty(ray_occ))
			pos->king_attack_map |= ray | lsb(bishops);
		else if (is_singular(ray_occ)
		    && is_nonempty(ray & pinnable_pieces))
			pos->bpin_map |= ray | lsb(bishops);
	}
}

static void
search_rook_king_attacks(struct position *pos)
{
	uint64_t rooks = pos->map[opponent_rook] | pos->map[opponent_queen];
	uint64_t ep_bit = EMPTY;

	if (pos_has_ep_target(pos))
		ep_bit = bit64(pos->ep_index);
	rooks &= rook_pattern_table[pos->king_index];
	for (; is_nonempty(rooks); rooks = reset_lsb(rooks)) {
		uint64_t ray = ray_table[pos->king_index][bsf(rooks)];
		uint64_t ray_occ = ray & pos->occupied;

		if (is_empty(ray_occ))
			pos->king_attack_map |= ray | lsb(rooks);
		else if (is_singular(ray_occ) && is_nonempty(ray & pos->map[0]))
			pos->rpin_map |= ray | lsb(rooks);
		else if (is_nonempty(ep_bit & ray)) {
			if (ind_file(pos->ep_index) != file_a
			    && is_nonempty(pos->map[pawn] & west_of(ep_bit))) {
				if ((ray & pos->occupied)
				    == (ep_bit | west_of(ep_bit))) {
					ep_bit = 0;
					pos->ep_index = 0;
				}
			}
			/*
			 * at this point in make_move, the en passant index is
			 * only set, if there is at least one pawn in the
			 * right place to make the capture, so there is
			 * no need to check
			 * if (ind_file(pos->ep_index) != file_h
			 * && is_nonempty(pos->map[pawn] & east_of(ep_bit)))
			 */
			else if ((ray & pos->occupied)
			    == (ep_bit | east_of(ep_bit))) {
				ep_bit = 0;
				pos->ep_index = 0;
			}
		}
	}
}

static void
search_knight_king_attacks(struct position *pos)
{
	pos->king_attack_map =
	    knight_pattern(pos->king_index) & pos->map[opponent_knight];
}

static void
search_pawn_king_attacks(struct position *pos)
{
	pos->king_attack_map =
	    pawn_attacks_player(pos->map[king]) & pos->map[opponent_pawn];
}

static void
generate_attack_maps(struct position *pos)
{
	generate_player_attacks(pos);
	generate_opponent_attacks(pos);
	pos->king_attack_map = EMPTY;
	pos->rpin_map = EMPTY;
	pos->bpin_map = EMPTY;
	search_pawn_king_attacks(pos);
	search_knight_king_attacks(pos);
	search_bishop_king_attacks(pos);
	search_rook_king_attacks(pos);
}

struct position*
position_allocate(void)
{
	struct position *pos = xaligned_alloc(pos_alignment, sizeof *pos);
	memset(pos, 0, sizeof *pos);
	return pos;
}

static bool
kings_valid(const struct position *pos)
{
	if (!is_singular(pos->map[king]))
		return false;
	if (!is_singular(pos->map[opponent_king]))
		return false;

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
	if (popcnt(pawns) > 8)
		return false;
	if (popcnt(opponent_pawns) > 8)
		return false;
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

int
position_reset(struct position *pos,
		const char board[static 64],
		const bool castle_rights[static 4],
		int ep_index)
{
	struct position dummy;

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
	setup_zhash(pos);
	return 0;
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

#if defined(TALTOS_CAN_USE_INTEL_AVX2)
static __m256i bitboard_flip_shufflekey_32;
#elif defined(TALTOS_CAN_USE_INTEL_AVX)
static __m128i bitboard_flip_shufflekey_16;
#endif

void
setup_registers(void)
{
/* BEGIN CSTYLED */
#if defined(TALTOS_CAN_USE_INTEL_AVX2)
	bitboard_flip_shufflekey_32 =
	    _mm256_setr_epi8(15, 14, 13, 12, 11, 10,  9,  8,
	                      7,  6,  5,  4,  3,  2,  1,  0,
	                     15, 14, 13, 12, 11, 10,  9,  8,
	                      7,  6,  5,  4,  3,  2,  1,  0);
#elif defined(TALTOS_CAN_USE_INTEL_AVX)
	bitboard_flip_shufflekey_16 =
	    _mm_setr_epi8(15, 14, 13, 12, 11, 10,  9,  8,
	                   7,  6,  5,  4,  3,  2,  1,  0);
#endif

#ifdef HAS_YMM_ZERO
	__asm__ volatile ("vpxor %xmm7, %xmm7, %xmm7");
#endif
#ifdef HAS_XMM_SHUFFLE_CONTROL_MASK_32
	__asm__ volatile
	    ("vmovdqa %0, %%ymm6":: "m" (bitboard_flip_shufflekey_32));
#elif defined(HAS_XMM_SHUFFLE_CONTROL_MASK_16)
	__asm__ volatile
	    ("vmovdqa %0, %%xmm6":: "m" (bitboard_flip_shufflekey_16));
#endif
/* END CSTYLED */
}


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
 * player's bitboard:     .1....1.  -->  ...1....
 *                        ..1..1..       ...1....
 *                        ...11...       ...1....
 *                        ...11...       ...1....
 *                        ..1..1..       ...1....
 *                        .1....1.       ..1.1...
 *                        1......1       .1...1..
 *                        ........       1.....1.
 *
 * opponent's bitboard:   1.....1.       ........
 *                        .1...1..       1......1
 *                        ..1.1...       .1....1.
 *                        ...1....       ..1..1..
 *                        ...1....       ...11...
 *                        ...1....       ...11...
 *                        ...1....       ..1..1..
 *                        ...1....       .1....1.
 *
 *
 */
#ifdef TALTOS_CAN_USE_INTEL_AVX2

static_assert(offsetof(struct position, attack) % 32 == 16, "alignment error");

static void
flip_piece_maps(struct position *restrict dst,
		const struct position *restrict src)
{
	const __m256i *restrict src32 = (const __m256i*)(src->attack + 2);
	__m256i *restrict dst32 = (__m256i*)(dst->attack + 2);

	for (int i = 0; i < 7; ++i) {
#ifdef HAS_XMM_SHUFFLE_CONTROL_MASK_32
		__asm__ volatile
		    ("vpshufb %%ymm6, %1, %0"
		    : "=x" (dst32[i])
		    : "x" (src32[i]));
#else
		dst32[i] =
		    _mm256_shuffle_epi8(src32[i], bitboard_flip_shufflekey_32);
#endif
	}
	dst32[7] =
	    (__m256i)_mm256_permute_pd((__m256d)(src32[7]), 1 + (1 << 2));
}

#elif defined(TALTOS_CAN_USE_INTEL_SHUFFLE_EPI8)

static void
flip_piece_maps(struct position *restrict dst,
		const struct position *restrict src)
{
	const __m128i attribute(align_value(16)) *restrict src16
	    = (const __m128i*)(src->attack + 2);
	__m128i attribute(align_value(16)) *restrict dst16
	    = (__m128i*)(dst->attack + 2);

	for (int i = 0; i < 14; ++i) {
#ifdef HAS_XMM_SHUFFLE_CONTROL_MASK_16
		__asm__ volatile
		    ("vpshufb %%xmm6, %1, %0"
		    : "=x" (dst16[i])
		    : "x" (src16[i]));
#else
		dst16[i] =
		    _mm_shuffle_epi8(src16[i], bitboard_flip_shufflekey_16);
#endif
	}

#ifdef TALTOS_CAN_USE_INTEL_AVX

	static_assert(offsetof(struct position, attack) % 32 == 16,
	    "alignment error");
	static_assert(offsetof(struct position, zhash) % 32 == 0,
	    "alignment error");

	__m256d *dst32 = (__m256d*)(dst->zhash);
	const __m256d *src32 = (const __m256d*)(src->zhash);

	*dst32 = _mm256_permute_pd(*src32, 1 + (1 << 2));
#else
	dst16[14] = _mm_shuffle_epi32(src16[14], (1 << 4) | (3 << 2) | 2);
	dst16[15] = _mm_shuffle_epi32(src16[15], (1 << 4) | (3 << 2) | 2);
#endif
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
	dst->zhash[0] = src->zhash[1];
	dst->zhash[1] = src->zhash[0];
	dst->cr_king_side = src->cr_opponent_king_side;
	dst->cr_queen_side = src->cr_opponent_queen_side;
	dst->cr_opponent_king_side = src->cr_king_side;
	dst->cr_opponent_queen_side = src->cr_queen_side;
}
#endif

static void
flip_castle_rights_move(struct position *pos, move m)
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

	__asm__ volatile
	    ("vmovdqa %%ymm7, %0"
	    :"=m" (*((__m256i*)(&(pos->king_attack_map)))));

#else

	// pos->king_attack_map = EMPTY;
	pos->rpin_map = EMPTY;
	pos->bpin_map = EMPTY;
	pos->ep_index = 0;

#endif
}

void
position_flip(struct position *restrict dst,
		const struct position *restrict src)
{
	assert(!pos_has_ep_target(src));
	dst->ep_index = 0;
	flip_board(dst, src);
	// todo: fix this!!!!
	flip_piece_maps(dst, src);
	clear_extra_bitboards(dst);
	dst->occupied = dst->map[0] | dst->map[1];
	dst->king_index = bsf(dst->map[king]);
	generate_attack_maps(dst);
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
		z2_toggle_sq(pos->zhash, i, pos->board[i], 0);
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
	z2_toggle_sq(pos->zhash, mfrom(m), pawn, 1);
	z2_toggle_sq(pos->zhash, mto(m), pawn, 1);
	if (mtype(m) == mt_en_passant) {
		pos->board[mto(m) + NORTH] = 0;
		pos->map[pawn] &= ~bit64(mto(m) + NORTH);
		pos->map[0] &= ~bit64(mto(m) + NORTH);
		z2_toggle_sq(pos->zhash, mto(m) + NORTH, pawn, 0);
		pos->material_value -= pawn_value;
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
		pos->map[1] ^= SQ_H8 | SQ_F8 | SQ_E8 | SQ_G8;
		z2_toggle_sq(pos->zhash, sq_h8, rook, 1);
		z2_toggle_sq(pos->zhash, sq_f8, rook, 1);
		z2_toggle_sq(pos->zhash, sq_e8, king, 1);
		z2_toggle_sq(pos->zhash, sq_g8, king, 1);
	}
	else if (m == flip_m(mcastle_queen_side)) {
		pos->board[sq_e8] = 0;
		pos->board[sq_c8] = king;
		pos->board[sq_d8] = rook;
		pos->board[sq_a8] = 0;
		pos->map[opponent_rook] ^= SQ_D8 | SQ_A8;
		pos->map[opponent_king] ^= SQ_E8 | SQ_C8;
		pos->map[1] ^= SQ_D8 | SQ_A8 | SQ_C8 | SQ_E8;
		z2_toggle_sq(pos->zhash, sq_d8, rook, 1);
		z2_toggle_sq(pos->zhash, sq_a8, rook, 1);
		z2_toggle_sq(pos->zhash, sq_e8, king, 1);
		z2_toggle_sq(pos->zhash, sq_c8, king, 1);
	}
	else {
		uint64_t from = mfrom64(m);
		uint64_t to = mto64(m);

		if (mtype(m) == mt_promotion)
			pos->material_value
			    -= piece_value[mresultp(m)] - pawn_value;
		enum piece porig = pos_piece_at(pos, mfrom(m));
		z2_toggle_sq(pos->zhash, mfrom(m), porig, 1);
		z2_toggle_sq(pos->zhash, mto(m), mresultp(m), 1);
		pos->map[porig + 1] &= ~from;
		pos->map[mresultp(m) + 1] |= to;
		pos->board[mfrom(m)] = 0;
		pos->board[mto(m)] = mresultp(m);
		pos->map[1] &= ~from;
		pos->map[1] |= to;
	}
}

static void
generate_player_attacks_move(struct position *pos, move m, uint64_t mmap)
{
	uint64_t all_attacks;

	all_attacks = pos->attack[pawn] =
	    pawn_attacks_player(pos->map[pawn]);
	all_attacks |= pos->attack[king];
	if (mcapturedp(m) == knight)
		pos->attack[knight] = knight_attacks(pos->map[knight]);
	all_attacks |= pos->attack[knight];
	if (((mcapturedp(m) & 2) == 0)
	    || is_nonempty(mmap & pos->sliding_attacks[0])) {
		pos->sliding_attacks[0] = pos->attack[bishop] =
		    bishop_attacks(pos->map[bishop], pos->occupied);
		pos->sliding_attacks[0] |= pos->attack[rook] =
		    rook_attacks(pos->map[rook], pos->occupied);
		pos->sliding_attacks[0] |= pos->attack[queen] =
		    bishop_attacks(pos->map[queen], pos->occupied)
		    | rook_attacks(pos->map[queen], pos->occupied);
	}
	all_attacks |= pos->sliding_attacks[0];
	pos->attack[0] = all_attacks;
}

static void
generate_opponent_attacks_move(struct position *pos, move m, uint64_t mmap)
{
	uint64_t occ = pos->occupied & ~pos->map[king];
	if (mresultp(m) == bishop ||
	    is_nonempty(mmap & pos->attack[opponent_bishop] & ~EDGES)) {
		pos->attack[opponent_bishop] =
		    bishop_attacks_nonempty(pos->map[opponent_bishop], occ);
	}
	pos->sliding_attacks[1] = pos->attack[opponent_bishop];
	if (mresultp(m) == rook ||
	    is_nonempty(mmap & pos->attack[opponent_rook])) {
		pos->attack[opponent_rook] =
		    rook_attacks_nonempty(pos->map[opponent_rook], occ);
	}
	pos->sliding_attacks[1] |= pos->attack[opponent_rook];
	if (mresultp(m) == queen
	    || is_nonempty(mmap & pos->attack[opponent_queen])) {
		pos->attack[opponent_queen] =
		    queen_attacks_nonempty(pos->map[opponent_queen], occ);
	}
	pos->sliding_attacks[1] |= pos->attack[opponent_queen];

	pos->attack[opponent_pawn] =
	    pawn_attacks_opponent(pos->map[opponent_pawn]);
	if ((mresultp(m) & 2) != 0) {
		pos->attack[opponent_king] =
		    king_moves_table[bsf(pos->map[opponent_king])];
		if (mresultp(m) == knight)
			pos->attack[opponent_knight] =
			    knight_attacks_nonempty(pos->map[opponent_knight]);
	}

	pos->attack[1] = pos->attack[opponent_pawn]
	    | pos->attack[opponent_king]
	    | pos->attack[opponent_knight]
	    | pos->sliding_attacks[1];
}

static void
generate_attacks_move(struct position *restrict dst,
			const struct position *restrict src,
			move m)
{
	uint64_t move_map = dst->occupied ^ bswap(src->occupied);

	if (is_capture(m) || is_nonempty(move_map & dst->sliding_attacks[0]))
		generate_player_attacks_move(dst, m, move_map);
	else
		dst->attack[0] = bswap(src->attack[1]);

	generate_opponent_attacks_move(dst, m, move_map);
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

	clear_to_square(dst, mto(m));
	if (mresultp(m) == pawn) {
		dst->king_index = bsf(dst->map[king]);
		move_pawn(dst, m);
		search_pawn_king_attacks(dst);
	}
	else {
		move_piece(dst, m);
		flip_castle_rights_move(dst, m);
		dst->king_index = bsf(dst->map[king]);
		search_knight_king_attacks(dst);
	}
	dst->occupied = dst->map[0] | dst->map[1];
	dst->opponent_material_value = -dst->material_value;
	search_bishop_king_attacks(dst);
	search_rook_king_attacks(dst);
	generate_attacks_move(dst, src, m);
}

void
position_make_move(struct position *restrict dst,
			const struct position *restrict src,
			move m)
{
	setup_registers();
	make_move(dst, src, m);
}