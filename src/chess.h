
#ifndef CHESS_H
#define CHESS_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

enum move_notation_type
{
	mn_coordinate,
	mn_san
};

typedef enum move_notation_type move_notation_type;

enum piece {
    nonpiece = 0,
    pawn =     1,
    king =     2,
    knight =   4,
    rook =     3,
    bishop =   6,
    queen =    7
};

typedef enum piece piece;

static inline bool is_bishop(piece p)
{
    return (p & bishop) == bishop;
}

static inline bool is_rook(piece p)
{
    return (p & rook) == rook;
}

enum move_type {
    pqueen = 0x8000 | (queen<<12),
    pknight = 0x8000 | (knight<<12),
    prook = 0x8000 | (rook<<12),
    pbishop = 0x8000 | (bishop<<12),
    castle_left = 0x1000,
    castle_right = 0x2000,
    en_passant = 0x3000,
    pawn_double_push = 0x4000,
    pawn_push = 0x5000,
    pawn_capture = 0x6000,
    general_move = 0x0000
};

typedef enum move_type move_type;

static inline bool is_valid_piece(int piece)
{
    return (piece == pawn)
        || (piece == king)
        || (piece == rook)
        || (piece == knight)
        || (piece == bishop)
        || (piece == queen);
}

static inline bool is_valid_mt(int t)
{
    return (t == general_move)
        || (t == en_passant)
        || (t == pawn_push)
        || (t == pawn_capture)
        || (t == pawn_double_push)
        || (t == castle_left)
        || (t == castle_right)
        || (t == pbishop)
        || (t == prook)
        || (t == pknight)
        || (t == pqueen);
}

enum player {
    white = 1,
    black = 0
};

typedef enum player player;

static inline player opponent(player p)
{
    assert(p == white || p == black);
    return p ^ (white|black);
}

enum sq_index_enum_set {
    sq_h1 = 56, sq_g1, sq_f1, sq_e1, sq_d1, sq_c1, sq_b1, sq_a1,
    sq_h2 = 48, sq_g2, sq_f2, sq_e2, sq_d2, sq_c2, sq_b2, sq_a2,
    sq_h7 = 8,  sq_g7, sq_f7, sq_e7, sq_d7, sq_c7, sq_b7, sq_a7,
    sq_h8 = 0,  sq_g8, sq_f8, sq_e8, sq_d8, sq_c8, sq_b8, sq_a8
};

typedef int move;

static const move mcastle_left = castle_left | sq_e1 | (sq_c1 << 6);
static const move mcastle_right = castle_right | sq_e1 | (sq_g1 << 6);

enum rank_t {
    rank_8 = 0, rank_7, rank_6, rank_5, rank_4, rank_3, rank_2, rank_1,
    rank_invalid
};

#define RSOUTH (+1)
#define RNORTH (-1)

typedef enum rank_t rank_t;

enum file_t {
    file_h = 0, file_g, file_f, file_e, file_d, file_c, file_b, file_a,
    file_invalid
};

#define FEAST (-1)
#define FWEST (+1)

typedef enum file_t file_t;

static inline bool is_valid_file(int file)
{
    return (file & ~7) == 0;
}

static inline bool is_valid_rank(int rank)
{
    return (rank & ~7) == 0;
}

static inline int ind(int rank, int file)
{
    assert(is_valid_rank(rank));
    assert(is_valid_file(file));

    return (rank << 3) + file;
}

#define NORTH (-8)
#define SOUTH (8)
#define WEST (1)
#define EAST (-1)

static inline unsigned ind_rank(int i)
{
    return i / 8;
}

static inline unsigned ind_file(int i)
{
    return i & 7;
}

static inline int flip_i(int i)
{
    return i ^ 0x38;
}

static inline bool ivalid(int i)
{
    return i >= 0 && i < 64;
}

static inline int mfrom(move m)
{
    return m & 0x3f;
}

static inline uint64_t mfrom64(move m)
{
    return UINT64_C(1) << mfrom(m);
}

static inline int mto(move m)
{
    return (m >> 6) & 0x3f;
}

static inline uint64_t mto64(move m)
{
    return UINT64_C(1) << mto(m);
}

static inline uint64_t m64(move m)
{
    return mfrom64(m) | mto64(m);
}

static inline move move_revert(move m)
{
    return (mfrom(m) << 6) | mto(m);
}

static inline bool move_match(move a, move b)
{
    return (a & 0xfff) == (b & 0xfff);
}

#if __STDC_VERSION__ >= 201112L

_Static_assert(((rook|knight|bishop|queen)<<12) == 0x7000,
            "piece vs move types assertion failed");

#endif

static inline move set_mt(move m, move_type mt)
{
    return m | mt;
}

static inline move set_promotion(move m, piece p)
{
    assert(p == queen || p == knight || p == rook || p == bishop);
    return 0x8000 | (p<<12) | m;
}

static inline move_type mtype(move m)
{
    return m & 0xf000;
}

static inline piece mpromotion(move m)
{
    return (m & 0x7000) >> 12;
}

static inline bool is_move_valid(move m)
{
    if (mfrom(m) == mto(m)) return false;
    switch (m & 0xf000) {
        case general_move:
        case en_passant:
            return true;
        case pawn_double_push:
            return ind_rank(mfrom(m)) == rank_2
                   && ind_rank(mto(m)) == rank_4
                   && ind_file(mfrom(m)) == ind_file(mto(m));
        case pqueen:
        case prook:
        case pbishop:
        case pknight:
            return (ind_rank(mto(m)) == rank_8)
                && ((mfrom(m) + NORTH == mto(m))
                        || (mfrom(m) + NORTH+WEST == mto(m))
                        || (mfrom(m) + NORTH+EAST == mto(m)));
        case castle_left:
            return m == mcastle_left;
        case castle_right:
            return m == mcastle_right;
        case pawn_push:
            return mto(m) == mfrom(m) + NORTH;
        case pawn_capture:
            return true;
        default:
            return false;
    }
}

static inline move madd_from(move m, int from)
{
    assert(ivalid(from));
    return m | from;
}

static inline move madd_to(move m, int to)
{
    assert(ivalid(to));
    return m | (to << 6);
}

static inline move mset_from(move m, int from)
{
    assert(ivalid(from));
    return (m & 0xffc0) | from;
}

static inline move mset_to(move m, int to)
{
    assert(ivalid(to));
    return (m & 0xf03f) | (to << 6);
}

static inline move flip_m(move m)
{
    return m ^ 0x0e38;
}

static inline move create_move(int from, int to)
{
    assert(from != to);
    return madd_to(madd_from(0, from), to);
}

static inline move
create_move_t(int from, int to, move_type t)
{
    assert(is_valid_mt(t));
    return create_move(from, to) | t;
}

static inline bool is_promotion(move m)
{
    return (m & 0x8000) != 0;
}

struct position;

#define NONE_MOVE 1
#define ILLEGAL_MOVE 2

#define MOVE_STR_BUFFER_LENGTH 16
#define FEN_BUFFER_LENGTH 128
#define BOARD_BUFFER_LENGTH 512
#define MOVE_ARRAY_LENGTH 168

#define PLY 2
#define MAX_PLY 128
#define MAX_Q_PLY 64

extern const char *start_position_fen;

int read_move(const struct position*, const char*, move*, player turn);
char *print_coor_move(move, char[static MOVE_STR_BUFFER_LENGTH], player turn);
char *print_move(const struct position*,
                move,
                char[static MOVE_STR_BUFFER_LENGTH],
                move_notation_type,
                player turn);
void setup_empty_position(struct position *);
char *position_print_fen(const struct position *,
                         char[static FEN_BUFFER_LENGTH],
                         unsigned full_move,
                         unsigned half_move,
                         player turn);
int position_read_fen(struct position *,
                      const char *buffer,
                      unsigned *full_move,
                      unsigned *half_move,
                      player *turn);
void board_print(char[static BOARD_BUFFER_LENGTH],
                 const struct position *,
                 player turn);
move *gen_plegal_moves(const struct position *, move[static MOVE_ARRAY_LENGTH]);
move *gen_moves(const struct position *, move[static MOVE_ARRAY_LENGTH]);
bool is_move_irreversible(const struct position *, move);
bool is_legal_move(const struct position *, move);
void position_copy(struct position *dst, const struct position *src);
void position_flip_ip(struct position *);
void position_flip(struct position *dst, const struct position *src);
void make_move(struct position *, move);
int make_plegal_move(struct position *, move);
struct position *position_create(void);
void position_destroy(struct position *);
void init_move_gen(void);
bool has_any_legal_move(const struct position *);
bool is_mate(const struct position *);
bool is_stalemate(const struct position *);

extern const char *start_position_fen;

#endif /* CHESS_H */

