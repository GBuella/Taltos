
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_CHESS_H
#define TALTOS_CHESS_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdalign.h>

#include "macros.h"

enum move_notation_type {
	mn_coordinate,
	mn_san
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

enum move_type {
	mt_general = 0,
	mt_pawn_double_push = 1 << 16,
	mt_castle_kingside = 2 << 16,
	mt_castle_queenside = 3 << 16,
	mt_en_passant = 4 << 16,
	mt_promotion = 5 << 16
};

static inline attribute(artificial) bool
is_valid_piece(int piece)
{
	return (piece == pawn)
	    || (piece == king)
	    || (piece == rook)
	    || (piece == knight)
	    || (piece == bishop)
	    || (piece == queen);
}

static inline attribute(artificial) bool
is_valid_mt(int t)
{
	return (t == mt_general)
	    || (t == mt_pawn_double_push)
	    || (t == mt_castle_kingside)
	    || (t == mt_castle_queenside)
	    || (t == mt_en_passant)
	    || (t == mt_promotion);
}

enum player {
	white = 0,
	black = 1
};

static inline attribute(artificial) int
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
	mcastle_king_side =
		sq_e1 | (sq_g1 << 8) | mt_castle_kingside | (king << 19),
	mcastle_queen_side =
		sq_e1 | (sq_c1 << 8) | mt_castle_queenside | (king << 19)
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

static inline attribute(artificial) bool
is_valid_file(int file)
{
	return (file & ~7) == 0;
}

static inline attribute(artificial) bool
is_valid_rank(int rank)
{
	return (rank & ~7) == 0;
}

static inline attribute(artificial) int
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

static inline attribute(artificial) uint64_t
east_of(uint64_t bit)
{
	return bit >> 1;
}

static inline attribute(artificial) uint64_t
west_of(uint64_t bit)
{
	return bit << 1;
}

static inline attribute(artificial) uint64_t
north_of(uint64_t bit)
{
	return bit >> 8;
}

static inline attribute(artificial) uint64_t
south_of(uint64_t bit)
{
	return bit << 8;
}

static inline attribute(artificial) unsigned
ind_rank(int i)
{
	return i / 8;
}

static inline attribute(artificial) unsigned
ind_file(int i)
{
	return i & 7;
}

static inline attribute(artificial) int
flip_i(int i)
{
	return i ^ 0x38;
}

static inline attribute(artificial) bool
ivalid(int i)
{
	return i >= 0 && i < 64;
}

typedef int32_t move;

static inline attribute(artificial) int
mfrom(move m)
{
	return m & 0xff;
}

static inline attribute(artificial) uint64_t
mfrom64(move m)
{
	return UINT64_C(1) << mfrom(m);
}

static inline attribute(artificial) int
mto(move m)
{
	return (m >> 8) & 0xff;
}

static inline attribute(artificial) uint64_t
mto64(move m)
{
	return UINT64_C(1) << mto(m);
}

static inline attribute(artificial) uint64_t
set_mfrom(move m, int from)
{
	return m | from;
}

static inline attribute(artificial) uint64_t
set_mto(move m, int to)
{
	return m | (to << 8);
}

static inline attribute(artificial) uint64_t
m64(move m)
{
	return mfrom64(m) | mto64(m);
}

static inline attribute(artificial) move
set_mt(move m, enum move_type mt)
{
	return m | mt;
}

static inline attribute(artificial) move
set_resultp(move m, enum piece p)
{
	invariant(is_valid_piece(p));
	return m | (p << 19);
}

static inline attribute(artificial) enum move_type
mtype(move m)
{
	return m & 0x70000;
}

static inline attribute(artificial) bool
is_promotion(move m)
{
	return mtype(m) == mt_promotion;
}

static inline attribute(artificial) enum piece
mresultp(move m)
{
	return (m >> 19) & 0xf;
}

static inline attribute(artificial) move
flip_m(move m)
{
	return m ^ 0x3838;
}

static inline attribute(artificial) move
set_capturedp(move m, int piece)
{
	invariant(piece == 0 || is_valid_piece(piece));
	return m | (piece << 24);
}

static inline attribute(artificial) enum piece
mcapturedp(move m)
{
	return m >> 24;
}

static inline attribute(artificial) bool
is_capture(move m)
{
	return mcapturedp(m) != 0;
}

#define MOVE_MASK (63 | (63 << 8) | (7 << 16) | (15 << 19) | (15 << 24))

static_assert(MOVE_MASK == 0x0f7f3f3f, "MOVE_MASK not as expected");

static inline attribute(artificial) move
create_move_t(int from, int to, enum move_type t, int result, int captured)
{
	return from | (to << 8) | t | (result << 19) | (captured << 24);
}

static inline attribute(artificial) move
create_move_g(int from, int to, int result, int captured)
{
	return create_move_t(from, to, mt_general, result, captured);
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

void init_move_gen(void);

bool has_any_legal_move(const struct position*)
	attribute(nonnull);

bool is_legal_move(const struct position*, move)
	attribute(nonnull);

bool is_mate(const struct position*)
	attribute(nonnull);

bool is_stalemate(const struct position*)
	attribute(nonnull);

#endif /* TALTOS_CHESS_H */
