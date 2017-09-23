/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#ifndef TALTOS_CHESS_H
#define TALTOS_CHESS_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "macros.h"

enum move_notation_type {
	mn_coordinate,
	mn_san,
	mn_fan
};

enum piece {
	nonpiece =	0,
	pawn =		2,
	rook =		4,
	king =		6,
	bishop =	8,
	knight =	10,
	queen =		12
};

enum {
	opponent_pawn = pawn|1,
	opponent_rook = rook|1,
	opponent_king = king|1,
	opponent_bishop = bishop|1,
	opponent_knight = knight|1,
	opponent_queen = queen|1
};

enum { player_mask = 1 };

static inline bool
is_valid_piece(int piece)
{
	return (piece == pawn)
	    || (piece == king)
	    || (piece == rook)
	    || (piece == knight)
	    || (piece == bishop)
	    || (piece == queen);
}

enum player {
	white = 0,
	black = 1
};

static inline int
opponent_of(int p)
{
	return p ^ 1;
}

enum {
	sq_h1 = 56, sq_g1, sq_f1, sq_e1, sq_d1, sq_c1, sq_b1, sq_a1,
	sq_h2 = 48, sq_g2, sq_f2, sq_e2, sq_d2, sq_c2, sq_b2, sq_a2,
	sq_h3 = 40, sq_g3, sq_f3, sq_e3, sq_d3, sq_c3, sq_b3, sq_a3,
	sq_h4 = 32, sq_g4, sq_f4, sq_e4, sq_d4, sq_c4, sq_b4, sq_a4,
	sq_h5 = 24, sq_g5, sq_f5, sq_e5, sq_d5, sq_c5, sq_b5, sq_a5,
	sq_h6 = 16, sq_g6, sq_f6, sq_e6, sq_d6, sq_c6, sq_b6, sq_a6,
	sq_h7 = 8,  sq_g7, sq_f7, sq_e7, sq_d7, sq_c7, sq_b7, sq_a7,
	sq_h8 = 0,  sq_g8, sq_f8, sq_e8, sq_d8, sq_c8, sq_b8, sq_a8
};

enum {
	rank_8 = 0, rank_7, rank_6, rank_5, rank_4, rank_3, rank_2, rank_1,
	rank_invalid
};

#define RSOUTH (+1)
#define RNORTH (-1)

enum {
	file_h = 0, file_g, file_f, file_e, file_d, file_c, file_b, file_a,
	file_invalid
};

#define FEAST (-1)
#define FWEST (+1)

static inline bool
is_valid_file(int file)
{
	return (file & ~7) == 0;
}

static inline bool
is_valid_rank(int rank)
{
	return (rank & ~7) == 0;
}

static inline int
ind(int rank, int file)
{
	assert(is_valid_rank(rank));
	assert(is_valid_file(file));

	return (rank << 3) + file;
}

#define NORTH (-8)
#define SOUTH (8)
#define WEST (1)
#define EAST (-1)

static inline uint64_t
east_of(uint64_t bit)
{
	return bit >> 1;
}

static inline uint64_t
west_of(uint64_t bit)
{
	return bit << 1;
}

static inline uint64_t
north_of(uint64_t bit)
{
	return bit >> 8;
}

static inline uint64_t
south_of(uint64_t bit)
{
	return bit << 8;
}

static inline unsigned
ind_rank(int i)
{
	return i / 8;
}

static inline unsigned
ind_file(int i)
{
	return i & 7;
}

static inline int
flip_i(int i)
{
	return i ^ 0x38;
}

static inline bool
ivalid(int i)
{
	return i >= 0 && i < 64;
}

typedef int32_t move;

/*
 * move layout:
 *
 * from           : bits  0 -  5
 * to             : bits  6 - 11
 * result piece   : bits 12 - 14
 * captured piece : bits 15 - 17
 * type           : bits 18 - 20
 *
 *
 *
 */

enum move_bit_offsets {
	move_bit_off_from = 0,

	move_bit_off_to = move_bit_off_from + 6,

	move_bit_off_result = move_bit_off_to + 6,
	move_bit_off_result_bit1,
	move_bit_off_result_bit2,

	move_bit_off_captured,
	move_bit_off_captured_bit1,
	move_bit_off_captured_bit2,

	move_bit_off_type,
	move_bit_off_type_bit1,
	move_bit_off_type_bit2,

	move_bit_mask_plus_one_shift
};

#define MOVE_MASK ((1 << move_bit_mask_plus_one_shift) - 1)

enum move_type {
	mt_general = 0,
	mt_pawn_double_push = 1 << move_bit_off_type,
	mt_castle_kingside = 2 << move_bit_off_type,
	mt_castle_queenside = 3 << move_bit_off_type,
	mt_en_passant = 4 << move_bit_off_type,
	mt_promotion = 5 << move_bit_off_type
};

static inline bool
is_valid_mt(int t)
{
	return (t == mt_general)
	    || (t == mt_pawn_double_push)
	    || (t == mt_castle_kingside)
	    || (t == mt_castle_queenside)
	    || (t == mt_en_passant)
	    || (t == mt_promotion);
}

#define MOVE_TYPE_MASK (7 << move_bit_off_type)

enum {
	mcastle_king_side =
		(sq_e1 << move_bit_off_from) |
		(sq_g1 << move_bit_off_to) |
		mt_castle_kingside |
		(king << (move_bit_off_result - 1)),

	mcastle_queen_side =
		(sq_e1 << move_bit_off_from) |
		(sq_c1 << move_bit_off_to) |
		mt_castle_queenside |
		(king << (move_bit_off_result - 1))
};

static inline int
mfrom(move m)
{
	return (m >> move_bit_off_from) & 0x3f;
}

static inline uint64_t
mfrom64(move m)
{
	return UINT64_C(1) << mfrom(m);
}

static inline int
mto(move m)
{
	return (m >> move_bit_off_to) & 0x3f;
}

static inline uint64_t
mto64(move m)
{
	return UINT64_C(1) << mto(m);
}

static inline uint64_t
set_mfrom(move m, int from)
{
	return m | from;
}

static inline uint64_t
set_mto(move m, int to)
{
	return m | (to << move_bit_off_to);
}

static inline uint64_t
m64(move m)
{
	return mfrom64(m) | mto64(m);
}

static inline move
set_mt(move m, enum move_type mt)
{
	return m | mt;
}

static inline move
set_resultp(move m, enum piece p)
{
	invariant(is_valid_piece(p));
	return m | (p << (move_bit_off_result - 1));
}

static inline enum move_type
mtype(move m)
{
	return m & MOVE_TYPE_MASK;
}

static inline bool
is_promotion(move m)
{
	return mtype(m) == mt_promotion;
}

static inline enum piece
mresultp(move m)
{
	return (m >> (move_bit_off_result - 1)) & 0xe;
}

static inline bool
is_under_promotion(move m)
{
	return is_promotion(m) && mresultp(m) != queen;
}

static inline move
flip_m(move m)
{
	return m ^ ((0x38 << move_bit_off_from) | (0x38 << move_bit_off_to));
}

static inline move
set_capturedp(move m, int piece)
{
	invariant(piece == 0 || is_valid_piece(piece));
	return m | (piece << (move_bit_off_captured - 1));
}

static inline enum piece
mcapturedp(move m)
{
	return (m >> (move_bit_off_captured - 1)) & 0xe;
}

static inline bool
is_capture(move m)
{
	return mcapturedp(m) != 0;
}

static inline bool
move_gives_check(move m)
{
	(void) m;
	return false;
}

static inline move
create_move_t(int from, int to, enum move_type t, int result, int captured)
{
	invariant(ivalid(from));
	invariant(ivalid(to));
	invariant(is_valid_piece(result));
	invariant(captured == 0 || is_valid_piece(captured));
	invariant(is_valid_mt(t));

	return (from << move_bit_off_from) |
	    (to << move_bit_off_to) |
	    t |
	    (result << (move_bit_off_result - 1)) |
	    (captured << (move_bit_off_captured - 1));
}

static inline move
create_move_g(int from, int to, int result, int captured)
{
	return create_move_t(from, to, mt_general, result, captured);
}

static inline move
create_move_pd(int from, int to)
{
	return create_move_t(from, to, mt_pawn_double_push, pawn, 0);
}

static inline move
create_move_pr(int from, int to, int result, int captured)
{
	return create_move_t(from, to, mt_promotion, result, captured);
}

static inline move
create_move_ep(int from, int to)
{
	return create_move_t(from, to, mt_en_passant, pawn, pawn);
}

static inline bool
is_move_valid(move m)
{
	return ivalid(mfrom(m)) && ivalid(mto(m))
	    && mfrom(m) != mto(m)
	    && is_valid_mt(mtype(m)) && is_valid_piece(mresultp(m))
	    && (mcapturedp(m) == 0 || is_valid_piece(mcapturedp(m)));
}

struct position;

enum {
	none_move = 1,
	illegal_move = 2
};

#define MOVE_STR_BUFFER_LENGTH 16
#define FEN_BUFFER_LENGTH 128
#define BOARD_BUFFER_LENGTH 512
#define MOVE_ARRAY_LENGTH 168

#define PLY 2
#define MAX_PLY 512
#define MAX_Q_PLY 512

extern const char *start_position_fen;



int position_square_at(const struct position*, int index)
	attribute(nonnull);

enum piece position_piece_at(const struct position*, int index)
	attribute(nonnull);

enum player position_player_at(const struct position*, int index)
	attribute(nonnull);

bool position_cr_king_side(const struct position*)
	attribute(nonnull);

bool position_cr_queen_side(const struct position*)
	attribute(nonnull);

bool position_cr_opponent_king_side(const struct position*)
	attribute(nonnull);

bool position_cr_opponent_queen_side(const struct position*)
	attribute(nonnull);

void get_position_key(const struct position*, uint64_t key[static 2])
	attribute(nonnull);

int fen_read_move(const char *fen, const char*, move*)
	attribute(nonnull(1, 2));

int read_move(const struct position*, const char*, move*, enum player)
	attribute(nonnull);

char *print_coor_move(move, char[static MOVE_STR_BUFFER_LENGTH], enum player)
	attribute(nonnull, returns_nonnull);

char *print_san_move(const struct position *pos, move m, char *str,
		     enum player turn);

char *print_fan_move(const struct position *pos, move m, char *str,
		     enum player turn);

char *print_move(const struct position*,
		move,
		char[static MOVE_STR_BUFFER_LENGTH],
		enum move_notation_type,
		enum player turn)
	attribute(nonnull, returns_nonnull);

char *position_print_fen(const struct position*,
		char[static FEN_BUFFER_LENGTH],
		int ep_index,
		enum player turn)
	attribute(nonnull, returns_nonnull);

char *position_print_fen_full(const struct position*,
		char[static FEN_BUFFER_LENGTH],
		int ep_target,
		unsigned full_move,
		unsigned half_move,
		enum player turn)
	attribute(nonnull, returns_nonnull);

const char *position_read_fen(struct position*, const char*,
		int *ep_index, enum player*)
	attribute(nonnull(2));

const char *position_read_fen_full(struct position*,
		const char *buffer,
		int *ep_target,
		unsigned *full_move,
		unsigned *half_move,
		enum player *turn)
	attribute(nonnull, warn_unused_result);

unsigned gen_moves(const struct position*,
		move[static MOVE_ARRAY_LENGTH])
	attribute(nonnull);

unsigned gen_captures(const struct position*,
		move[static MOVE_ARRAY_LENGTH])
	attribute(nonnull);

bool is_move_irreversible(const struct position*, move)
	attribute(nonnull);

struct position *position_dup(const struct position*)
	attribute(nonnull, warn_unused_result, malloc);

void position_flip(struct position *restrict dst,
		const struct position *restrict src)
	attribute(nonnull);

void position_make_move(struct position *restrict dst,
		const struct position *restrict src,
		move)
	attribute(nonnull);

struct position *position_allocate(void)
	attribute(returns_nonnull, warn_unused_result, malloc);

struct position *position_create(const char board[static 64],
		const bool castle_rights[static 4],
		int en_passant_index)
	attribute(nonnull, warn_unused_result, malloc);

enum castle_right_indices {
	cri_king_side,
	cri_queen_side,
	cri_opponent_king_side,
	cri_opponent_queen_side,
};

bool position_has_en_passant_index(const struct position*)
	attribute(nonnull);

int position_get_en_passant_index(const struct position*)
	attribute(nonnull);

int position_reset(struct position*,
		const char board[static 64],
		const bool castle_rights[static 4],
		int en_passant_index)
	attribute(nonnull(2, 3), warn_unused_result);

void position_destroy(struct position*);

bool has_any_legal_move(const struct position*)
	attribute(nonnull);

bool is_legal_move(const struct position*, move)
	attribute(nonnull);

bool pos_is_check(const struct position*)
	attribute(nonnull);

bool pos_has_insufficient_material(const struct position*)
	attribute(nonnull);

bool pos_equal(const struct position*, const struct position*)
	attribute(nonnull);

#endif /* TALTOS_CHESS_H */
