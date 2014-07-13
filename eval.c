
#include <stdio.h>

#include "bitmanipulate.h"
#include "position.h"
#include "search.h"

const int piece_value[8] = {
    0, PAWN_VALUE, MATE_VALUE, ROOK_VALUE,
    KNIGHT_VALUE, 0, BISHOP_VALUE, QUEEN_VALUE
};

#define DOUBLE_PANW_VALUE 2
#define ISOLATED_PAWN_PENALTY 2
#define CENTER_RANGE_ATTACK_VALUE 1
#define CENTER_PAWN_ATTAK_VALUE 2
#define KNIGHT_OUTPOST_VALUE 2
#define KNIGHT_CENTER_SQ_VALUE 3
#define KINGS_PAWN_GUARD_VALUE 1;
#define KINGS_PAWN_VALUE 2
#define KING_RANK_12_VALUE 2
#define CASTLE_BONUS_VALUE 1
#define ROOK_AT_FILE_VALUE 3

#define MAX_POSITIONAL_VALUE 0x70

static int add_material(uint64_t map, const uint64_t bb[static 5], int value)
{
    return ((spopcnt(map & side1(bb)) - spopcnt(map & side0(bb))) * value);
}

int eval_material(const uint64_t bb[static 5])
{
    int value = 0;

    value += add_material(bb_pawns_map(bb), bb, PAWN_VALUE);
    value += add_material(bb_knights_map(bb), bb, KNIGHT_VALUE);
    value += add_material(bb_rooks_map(bb), bb, ROOK_VALUE);
    value += add_material(bb_bishops_map(bb), bb, BISHOP_VALUE);
    value += add_material(bb_queens_map(bb), bb, XQUEEN_VALUE);
    return value;
}


static uint64_t
gen_range(uint64_t occ, uint64_t src_map, const struct magical *magics)
{
    uint64_t map = EMPTY;

    while (nonempty(src_map)) {
        map |= sliding_map(occ, magics + bsf(src_map));
        src_map = reset_lsb(src_map);
    }
    return map;
}

#define END_MAX 2

int compute_endgame_factor(const uint64_t bb[static 5])
{
    if (popcnt(bb[1]|bb[2]) > 9) return 0;
    if (popcnt(bb[1]|bb[2]) > 5) return 1;
    return END_MAX;
}

static int eval_pawn_structure(uint64_t pawns0, uint64_t pawns1,
                               uint64_t *outposts1, uint64_t *outposts0)
{
    int value;
    uint64_t reach1 = kogge_stone_north(pawns1);
    uint64_t reach0 = kogge_stone_south(pawns0);
    uint64_t isolated1, isolated0;

    value = spopcnt(pawn_attacks1(pawns1) & CENTER_SQ)
                * CENTER_PAWN_ATTAK_VALUE;
    value -= spopcnt(pawn_attacks0(pawns0) & CENTER_SQ)
                * CENTER_PAWN_ATTAK_VALUE;
    *outposts1 = pawn_attacks1(pawns1) & ~reach0;
    *outposts0 = pawn_attacks0(pawns0) & ~reach1;
    uint64_t files1 = kogge_stone_south(reach1);
    uint64_t files0 = kogge_stone_south(reach0);
    value += (spopcnt(files0 & 0xff) - spopcnt(pawns0)) * DOUBLE_PANW_VALUE;
    value -= (spopcnt(files1 & 0xff) - spopcnt(pawns1)) * DOUBLE_PANW_VALUE;
    isolated1 = files1 & ~((files1 << 8) & ~FILE_H) & ~((files1 >> 8) & FILE_A);
    isolated1 &= pawns1;
    isolated0 = files0 & ~((files0 << 8) & ~FILE_H) & ~((files0 >> 8) & FILE_A);
    isolated0 &= pawns0;
    value += (spopcnt(isolated0) - spopcnt(isolated1)) * ISOLATED_PAWN_PENALTY;
    return value;
}

static int eval_endgame(const uint64_t board[static 5],
                        uint64_t ranged_1, uint64_t ranged_0)
{
    int value = 0;
    uint64_t pawns0 = bb_pawns_map0(board);
    uint64_t pawns1 = bb_pawns_map1(board);

    value += spopcnt(pawns1 & (RANK_5|RANK_6|RANK_7));
    value += spopcnt(pawns1 & (RANK_6|RANK_7));
    value += spopcnt(pawns1 & RANK_7);
    value -= spopcnt(pawns0 & (RANK_4|RANK_3|RANK_2));
    value -= spopcnt(pawns0 & (RANK_3|RANK_2));
    value -= spopcnt(pawns0 & RANK_2);
    value += spopcnt(((pawns1 & (RANK_6|RANK_7)) >> 8) & ranged_0);
    value -= spopcnt(((pawns0 & (RANK_2|RANK_1)) << 8) & ranged_1);
    value += spopcnt(((pawns1 & (RANK_5|RANK_6)) >> 16) & ranged_0);
    value -= spopcnt(((pawns0 & (RANK_3|RANK_2)) << 16) & ranged_1);
    return value;
}

static int king_fortress(uint64_t pawns, uint64_t rooks, uint64_t king)
{
    int value;
    uint64_t guard;

    if (empty(king & (RANK_1|RANK_2))) {
        return -KING_RANK_12_VALUE;
    }
    guard = (king >> 8) | ((king >> 9) & ~FILE_A) | ((king >> 7) & ~FILE_H);
    value = spopcnt((guard | (guard >> 8)) & pawns) * KINGS_PAWN_GUARD_VALUE;
    if (king == SQ_C1 && empty(SQ_A1 & rooks)) {
        value += CASTLE_BONUS_VALUE;
    }
    else if (king == SQ_G1 && empty(SQ_H1 & rooks)) {
        value += CASTLE_BONUS_VALUE;
    }
    return value;
}

static int eval_middlegame(const struct position *pos,
                           uint64_t ranged_1, uint64_t ranged_0,
                           uint64_t outposts1, uint64_t outposts0)
{
    int value = 0;

    value += (spopcnt(ranged_1 & CENTER_SQ) - spopcnt(ranged_0 & CENTER_SQ))
                * CENTER_RANGE_ATTACK_VALUE;
    value += add_material(CENTER_SQ & knights_map1(pos),
                            pos->bb, KNIGHT_CENTER_SQ_VALUE);
    value -= add_material(CENTER_SQ & knights_map0(pos),
                            pos->bb, KNIGHT_CENTER_SQ_VALUE);
    value += (spopcnt(pos->king_reach_map_0) - 3) / 2;
    value -= (spopcnt(pos->king_reach_map_1) - 3) / 2;
    value += king_fortress(pawns_map1(pos),
                           rooks_only_map1(pos),
                           king_map1(pos));
    value -= king_fortress(bswap(pawns_map1(pos)),
                           bswap(rooks_only_map1(pos)),
                           bswap(king_map1(pos)));
    value += spopcnt(CENTER_SQ & outposts1 & knights_map1(pos))
                * KNIGHT_OUTPOST_VALUE;
    value -= spopcnt(CENTER_SQ & outposts0 & knights_map0(pos))
                * KNIGHT_OUTPOST_VALUE;
    return value;
}

int eval(const struct node *node)
{
    assert(node != NULL);

    int value;
    const struct position *pos = node->pos;
    const uint64_t *board = node->pos->bb;
    uint64_t ranged_1, ranged_0;
    uint64_t occ = occupied(pos);
    int end = compute_endgame_factor(board);

    ranged_1 = gen_range(occ, rooks_map1(pos), rook_magics);
    ranged_1 |= gen_range(occ, bishops_map1(pos), bishop_magics);
    ranged_0 = gen_range(occ, rooks_map0(pos), rook_magics);
    ranged_0 |= gen_range(occ, bishops_map0(pos), bishop_magics);
    value = eval_material(board);
    value += (spopcnt(ranged_1) - spopcnt(ranged_0)) / 2;

    if (false && end > 0) {
        value += end * eval_endgame(board, ranged_1, ranged_0);
    }
    if (false && end < END_MAX) {
        uint64_t outposts1, outposts0;

        value += (END_MAX - end) *
                    eval_pawn_structure(pawns_map0(node->pos),
                                        pawns_map1(node->pos),
                                        &outposts1, &outposts0);
        value += (END_MAX - end) *
                        eval_middlegame(pos, ranged_1, ranged_0,
                                        outposts1, outposts0);
    }
    
    return value;
}

