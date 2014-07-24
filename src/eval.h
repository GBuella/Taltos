
#ifndef EVAL_H
#define EVAL_H

#include <stdint.h>

#define MAX_VALUE 0x7ff
#define MATE_VALUE (MAX_VALUE - 128)
#define PAWN_VALUE 0x10
#define KNIGHT_VALUE 0x30
#define BISHOP_VALUE 0x31
#define ROOK_VALUE 0x50
#define QUEEN_VALUE 0x90
#define XQUEEN_VALUE (QUEEN_VALUE - BISHOP_VALUE - ROOK_VALUE)

extern const int piece_value[8];

struct position;

int eval(const struct position *);
int eval_material(const uint64_t[static 5]);

struct eval_factors {
    int material;
    int middle_game;
    int end_game;
    int basic_mobility;
    int pawn_structure;
    int passed_pawn_score;
    int king_fortress;
    int piece_placement;
};

typedef struct eval_factors eval_factors;

eval_factors compute_eval_factors(const struct position *);

#endif
