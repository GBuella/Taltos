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

#ifndef TALTOS_CHESS_H
#define TALTOS_CHESS_H

#include <cstdint>

#include "macros.h"

namespace taltos
{

enum move_notation_type {
	mn_coordinate,
	mn_san,
	mn_fan
};

constexpr int nonpiece = 0;
constexpr int pawn = 2;
constexpr int rook = 4;
constexpr int king = 6;
constexpr int bishop = 8;
constexpr int knight = 10;
constexpr int queen = 12;

constexpr int opponent_pawn = pawn | 1;
constexpr int opponent_rook = rook | 1;
constexpr int opponent_king = king | 1;
constexpr int opponent_bishop = bishop | 1;
constexpr int opponent_knight = knight | 1;
constexpr int opponent_queen = queen | 1;

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

static inline int opponent_of(int p)
{
	return p ^ 1;
}

static inline enum player opponent_of(enum player p)
{
	if (p == white)
		return black;
	else
		return white;
}


enum {
	h1 = 56, g1, f1, e1, d1, c1, b1, a1,
	h2 = 48, g2, f2, e2, d2, c2, b2, a2,
	h3 = 40, g3, f3, e3, d3, c3, b3, a3,
	h4 = 32, g4, f4, e4, d4, c4, b4, a4,
	h5 = 24, g5, f5, e5, d5, c5, b5, a5,
	h6 = 16, g6, f6, e6, d6, c6, b6, a6,
	h7 = 8,  g7, f7, e7, d7, c7, b7, a7,
	h8 = 0,  g8, f8, e8, d8, c8, b8, a8
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

constexpr int mt_general = 0;
constexpr int mt_pawn_double_push = 1 << (int)move_bit_off_type;
constexpr int mt_castle_kingside = 2 << (int)move_bit_off_type;
constexpr int mt_castle_queenside = 3 << (int)move_bit_off_type;
constexpr int mt_en_passant = 4 << (int)move_bit_off_type;
constexpr int mt_promotion = 5 << (int)move_bit_off_type;

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

constexpr move mcastle_king_side =
		(e1 << move_bit_off_from) |
		(g1 << move_bit_off_to) |
		mt_castle_kingside |
		(king << (move_bit_off_result - 1));

constexpr move mcastle_queen_side =
		(e1 << move_bit_off_from) |
		(c1 << move_bit_off_to) |
		mt_castle_queenside |
		(king << (move_bit_off_result - 1));

static inline int
mfrom(move m)
{
	return (m >> move_bit_off_from) & 0x3f;
}

static inline int
mto(move m)
{
	return (m >> move_bit_off_to) & 0x3f;
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

static inline move
set_mt(move m, int mt)
{
	return m | mt;
}

static inline move
set_resultp(move m, int p)
{
	invariant(is_valid_piece(p));
	return m | (p << (move_bit_off_result - 1));
}

static inline int
mtype(move m)
{
	return m & MOVE_TYPE_MASK;
}

static inline bool
is_promotion(move m)
{
	return mtype(m) == mt_promotion;
}

static inline int
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

static inline int
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
create_move_t(int from, int to, int move_type, int result, int captured)
{
	invariant(ivalid(from));
	invariant(ivalid(to));
	invariant(is_valid_piece(result));
	invariant(captured == 0 || is_valid_piece(captured));
	invariant(is_valid_mt(move_type));

	return (from << move_bit_off_from) |
	    (to << move_bit_off_to) |
	    move_type |
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

constexpr int none_move = 1;

#define MOVE_STR_BUFFER_LENGTH 16
#define FEN_BUFFER_LENGTH 128
#define BOARD_BUFFER_LENGTH 512
#define MOVE_ARRAY_LENGTH 168

#define PLY 2
#define MAX_PLY 512
#define MAX_Q_PLY 512

extern const char *start_position_fen;



int position_square_at(const struct position*, int index);

int position_piece_at(const struct position*, int index);

int position_player_at(const struct position*, int index);

bool position_cr_king_side(const struct position*);

bool position_cr_queen_side(const struct position*);

bool position_cr_opponent_king_side(const struct position*);

bool position_cr_opponent_queen_side(const struct position*);

void get_position_key(const struct position*, uint64_t key[]);

int fen_read_move(const char *fen, const char*, move*);

int read_move(const struct position*, const char*, move*, enum player);

char *print_coor_move(move, char[], enum player);

char *print_san_move(const struct position *pos, move m, char *str, enum player turn);

char *print_fan_move(const struct position *pos, move m, char *str, enum player turn);

char *print_move(const struct position*,
		move,
		char[],
		int move_notation_type,
		enum player turn);

char *position_print_fen(const struct position*,
		char[],
		int ep_index,
		enum player turn);

char *position_print_fen_full(const struct position*,
		char[],
		int ep_target,
		unsigned full_move,
		unsigned half_move,
		enum player turn);

const char *position_read_fen(struct position*, const char*, int *ep_index, enum player *player);

const char *position_read_fen_full(struct position*,
		const char *buffer,
		int *ep_target,
		unsigned *full_move,
		unsigned *half_move,
		enum player *turn);

unsigned gen_moves(const struct position*, move[]);

unsigned gen_captures(const struct position*, move[]);

bool is_move_irreversible(const struct position*, move);

struct position *position_dup(const struct position*);

void position_flip(struct position *restrict dst,
		const struct position *restrict src);

void position_make_move(struct position *restrict dst,
		const struct position *restrict src,
		move);

struct position *position_allocate(void);

struct position *position_create(const char board[],
		const bool castle_rights[],
		int en_passant_index);

constexpr int cri_king_side = 0;
constexpr int cri_queen_side = 1;
constexpr int cri_opponent_king_side = 2;
constexpr int cri_opponent_queen_side = 3;

bool position_has_en_passant_index(const struct position*);

int position_get_en_passant_index(const struct position*);

int position_reset(struct position*,
		const char board[],
		const bool castle_rights[],
		int en_passant_index);

void position_destroy(struct position*);

bool has_any_legal_move(const struct position*);

bool is_legal_move(const struct position*, move);

bool pos_is_check(const struct position*);

bool pos_has_insufficient_material(const struct position*);

bool pos_equal(const struct position*, const struct position*);

}

#endif /* TALTOS_CHESS_H */
