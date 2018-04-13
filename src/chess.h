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

#ifndef TALTOS_CHESS_H
#define TALTOS_CHESS_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

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

enum player {
	white = 0,
	black = 1
};

enum {
	empty_squre = 0,
	white_pawn = pawn|white,
	white_rook = rook|white,
	white_king = king|white,
	white_bishop = bishop|white,
	white_knight = knight|white,
	white_queen = queen|white,
	black_pawn = pawn|black,
	black_rook = rook|black,
	black_king = king|black,
	black_bishop = bishop|black,
	black_knight = knight|black,
	black_queen = queen|black
};

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

static inline bool
is_valid_square(int square)
{
	return (square == empty_squre)
	    || (square == white_pawn)
	    || (square == white_rook)
	    || (square == white_king)
	    || (square == white_bishop)
	    || (square == white_knight)
	    || (square == white_queen)
	    || (square == black_pawn)
	    || (square == black_rook)
	    || (square == black_king)
	    || (square == black_bishop)
	    || (square == black_knight)
	    || (square == black_queen);
}

static inline enum player
square_player(int square)
{
	return square & (black|white);
}

static inline enum piece
square_piece(int square)
{
	return square & ~(black|white);
}

static inline enum player
opponent_of(enum player p)
{
	return p ^ 1;
}

typedef enum {
	h1 = 56, g1, f1, e1, d1, c1, b1, a1,
	h2 = 48, g2, f2, e2, d2, c2, b2, a2,
	h3 = 40, g3, f3, e3, d3, c3, b3, a3,
	h4 = 32, g4, f4, e4, d4, c4, b4, a4,
	h5 = 24, g5, f5, e5, d5, c5, b5, a5,
	h6 = 16, g6, f6, e6, d6, c6, b6, a6,
	h7 = 8,  g7, f7, e7, d7, c7, b7, a7,
	h8 = 0,  g8, f8, e8, d8, c8, b8, a8
} coordinate;

typedef enum {
	rank_8 = 0, rank_7, rank_6, rank_5, rank_4, rank_3, rank_2, rank_1,
	rank_invalid
} rank;

#define RSOUTH (+1)
#define RNORTH (-1)

typedef enum {
	file_h = 0, file_g, file_f, file_e, file_d, file_c, file_b, file_a,
	file_invalid
} file;

#define FEAST (-1)
#define FWEST (+1)

static inline bool
is_valid_file(file file)
{
	return (file & ~7) == 0;
}

static inline bool
is_valid_rank(rank rank)
{
	return (rank & ~7) == 0;
}

static inline int
ind(rank rank, file file)
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

static inline rank
ind_rank(coordinate i)
{
	return i / 8;
}

static inline file
ind_file(coordinate i)
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

enum move_type {
	mt_general,
	mt_pawn_double_push,
	mt_castle_kingside,
	mt_castle_queenside,
	mt_en_passant,
	mt_promotion
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

struct move {

#ifdef TALTOS_USE_NO_ENUM_BITFIELDS

	uint32_t from:8;
	uint32_t to:8;
	uint32_t result:4;
	uint32_t captured:4;
	uint32_t type:3;

#else

	coordinate from:8;
	coordinate to:8;
	enum piece result:4;
	enum piece captured:4;
	enum move_type type:3;

#endif

	uint32_t reserved:32 - 8 - 8 - 3 - 4 - 4;
};

#pragma GCC diagnostic pop

static_assert(sizeof(struct move) == sizeof(uint32_t), "Invalid struct move");

static inline uint32_t
movei(struct move m)
{
	uint32_t value;
	char *dst = (void*)&value;
	const char *src = (const void*)&m;

	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];

	return value;
}

static inline struct move
imove(uint32_t value)
{
	struct move m;
	char *dst = (void*)&m;
	const char *src = (const void*)&value;

	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];

	return m;
}

static inline bool
is_null_move(struct move m)
{
	return movei(m) == 0;
}

static inline struct move
null_move(void)
{
	return (struct move){0, 0, 0, 0, 0, 0};
}

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

static inline struct move
mcastle_white_king_side(void)
{
	return (struct move){.from = e1, .to = g1, .result = king,
	        .captured = 0, .type = mt_castle_kingside, .reserved = 0};
}

static inline struct move
mcastle_white_queen_side(void)
{
	return (struct move){.from = e1, .to = c1, .result = king,
	        .captured = 0, .type = mt_castle_queenside, .reserved = 0};
}

static inline struct move
mcastle_black_king_side(void)
{
	return (struct move){.from = e8, .to = g8, .result = king,
	        .captured = 0, .type = mt_castle_kingside, .reserved = 0};
}

static inline struct move
mcastle_black_queen_side(void)
{
	return (struct move){.from = e8, .to = c8, .result = king,
	        .captured = 0, .type = mt_castle_queenside, .reserved = 0};
}

static inline struct move
create_move_g(coordinate from, coordinate to,
	      enum piece result, enum piece captured)
{
	return (struct move){.from = from, .to = to, .result = result,
	        .captured = captured, .type = mt_general, .reserved = 0};
}

static inline struct move
create_move_ep(coordinate from, coordinate to)
{
	return (struct move){.from = from, .to = to, .result = pawn,
	        .captured = pawn, .type = mt_en_passant, .reserved = 0};
}

static inline struct move
create_move_pr(coordinate from, coordinate to,
	       enum piece result, enum piece captured)
{
	return (struct move){.from = from, .to = to, .result = result,
	        .captured = captured, .type = mt_promotion, .reserved = 0};
}

static inline struct move
create_move_pd(coordinate from, coordinate to)
{
	return (struct move){.from = from, .to = to, .result = pawn,
	        .captured = 0, .type = mt_pawn_double_push, .reserved = 0};
}

static inline uint64_t
mfrom64(struct move m)
{
	return UINT64_C(1) << m.from;
}

static inline uint64_t
mto64(struct move m)
{
	return UINT64_C(1) << m.to;
}

static inline uint64_t
m64(struct move m)
{
	return mfrom64(m) | mto64(m);
}

static inline bool
is_promotion(struct move m)
{
	return m.type == mt_promotion;
}

static inline bool
is_under_promotion(struct move m)
{
	return m.type == mt_promotion && m.result != queen;
}

static inline bool
is_capture(struct move m)
{
	return m.captured != 0;
}

static inline bool
is_move_valid(struct move m)
{
	return ivalid(m.from) && ivalid(m.to)
	    && m.from != m.to
	    && is_valid_mt(m.type) && is_valid_piece(m.result)
	    && (m.captured == 0 || is_valid_piece(m.captured));
}

static inline bool
is_pawn_move(struct move m)
{
	return m.result == pawn || m.type == mt_promotion;
}

static inline bool
move_eq(struct move a, struct move b)
{
	return movei(a) == movei(b);
}

struct position;

#define MOVE_STR_BUFFER_LENGTH 16
#define FEN_BUFFER_LENGTH 128
#define BOARD_BUFFER_LENGTH 512
#define MOVE_ARRAY_LENGTH 168

#define PLY 2
#define MAX_PLY 512
#define MAX_Q_PLY 512

extern const char *start_position_fen;



enum player position_turn(const struct position*)
	attribute(nonnull);

int position_square_at(const struct position*, coordinate)
	attribute(nonnull);

enum piece position_piece_at(const struct position*, coordinate)
	attribute(nonnull);

enum player position_player_at(const struct position*, coordinate)
	attribute(nonnull);

bool position_cr_white_king_side(const struct position*)
	attribute(nonnull);

bool position_cr_white_queen_side(const struct position*)
	attribute(nonnull);

bool position_cr_black_king_side(const struct position*)
	attribute(nonnull);

bool position_cr_black_queen_side(const struct position*)
	attribute(nonnull);

uint64_t get_position_key(const struct position*)
	attribute(nonnull);

unsigned position_full_move_count(const struct position*) attribute(nonnull);

unsigned position_half_move_count(const struct position*) attribute(nonnull);

int fen_read_move(const char *fen, const char*, struct move*)
	attribute(nonnull(1, 2));

int read_move(const struct position*, const char*, struct move*)
	attribute(nonnull);

char *print_coor_move(char[static MOVE_STR_BUFFER_LENGTH], struct move);

char *print_san_move(char[static MOVE_STR_BUFFER_LENGTH],
		     const struct position *pos, struct move);

char *print_fan_move(char[static MOVE_STR_BUFFER_LENGTH],
		     const struct position *pos, struct move);

char *print_move(char[static MOVE_STR_BUFFER_LENGTH],
		 const struct position*,
		 struct move,
		 enum move_notation_type)
	attribute(nonnull, returns_nonnull);

char *position_print_fen_no_move_count(char[static FEN_BUFFER_LENGTH],
				       const struct position*)
	attribute(nonnull, returns_nonnull);

char *position_print_fen(char[static FEN_BUFFER_LENGTH], const struct position*)
	attribute(nonnull, returns_nonnull);

const char *position_read_fen(const char*, struct position*)
	attribute(nonnull);

unsigned gen_moves(const struct position*,
		struct move[static MOVE_ARRAY_LENGTH])
	attribute(nonnull);

unsigned gen_captures(const struct position*,
		struct move[static MOVE_ARRAY_LENGTH])
	attribute(nonnull);

bool is_move_irreversible(const struct position*, struct move)
	attribute(nonnull);

struct position *position_dup(const struct position*)
	attribute(nonnull, warn_unused_result, malloc);

int position_flip(struct position *restrict dst,
		  const struct position *restrict src)
	attribute(nonnull);

void make_move(struct position *restrict dst,
	       const struct position *restrict src,
	       struct move)
	attribute(nonnull);

struct position *position_allocate(void)
	attribute(returns_nonnull, warn_unused_result, malloc);

struct position_desc {
	char board[64];
	bool castle_rights[4];
	coordinate en_passant_index;
	enum player turn;
	unsigned half_move_counter;
	unsigned full_move_counter;
};

int position_reset(struct position*, const struct position_desc)
	attribute(warn_unused_result);

struct position *position_create(const struct position_desc)
	attribute(returns_nonnull, warn_unused_result, malloc);

void position_destroy(struct position*);

enum castle_right_indices {
	cri_white_king_side,
	cri_white_queen_side,
	cri_black_king_side,
	cri_black_queen_side
};

// en passant index - where the opponent's pawn is
bool position_has_en_passant_index(const struct position*)
	attribute(nonnull);

int position_get_en_passant_index(const struct position*)
	attribute(nonnull);

// en passant target - north of the opponent's pawn (used in FEN)
bool position_has_en_passant_target(const struct position*)
	attribute(nonnull);

int position_get_en_passant_target(const struct position*)
	attribute(nonnull);

bool has_any_legal_move(const struct position*)
	attribute(nonnull);

bool is_legal_move(const struct position*, struct move)
	attribute(nonnull);

bool pos_is_check(const struct position*)
	attribute(nonnull);

bool pos_has_insufficient_material(const struct position*)
	attribute(nonnull);

bool pos_equal(const struct position*, const struct position*)
	attribute(nonnull);

enum player position_turn(const struct position*)
	attribute(nonnull);

#endif /* TALTOS_CHESS_H */
