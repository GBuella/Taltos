
#include "bitmanipulate.h"
#include "position.h"
#include "search.h"
#include "eval.h"

const int piece_value[8] = {
    0, PAWN_VALUE, MATE_VALUE, ROOK_VALUE,
    KNIGHT_VALUE, 0, BISHOP_VALUE, QUEEN_VALUE
};

#define PAWN_RANK_7_VALUE 3
#define PASSED_PAWN_VALUE 2
#define PASSED_PAWN_PATH_BLOCKED_PENALTY 1
#define PASSED_PAWN_PATH_ATTACKED_PENALTY 1
#define DOUBLE_PANW_PENALTY 2
#define ISOLATED_PAWN_PENALTY 2
#define CENTER_PAWN_ATTAK_VALUE 1
#define KNIGHT_OUTPOST_VALUE 2
#define KNIGHT_CENTER_VALUE 1
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

static uint64_t knights_attacks(uint64_t knights)
{
    uint64_t attacks = EMPTY;

    for (; nonempty(knights); knights = reset_lsb(knights)) {
        attacks |= knight_pattern(bsf(knights));
    }
    return attacks;
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

static int pawn_structure(uint64_t pawns0, uint64_t pawns1,
                               uint64_t *outposts1, uint64_t *outposts0)
{
    int value = 0;
    uint64_t reach1 = kogge_stone_north(pawns1);
    uint64_t reach0 = kogge_stone_south(pawns0);
    uint64_t isolated1, isolated0;

    *outposts1 = pawn_attacks1(pawns1) & ~reach0;
    *outposts0 = pawn_attacks0(pawns0) & ~reach1;
    uint64_t files1 = kogge_stone_south(reach1);
    uint64_t files0 = kogge_stone_north(reach0);
    value += (spopcnt(pawns0) - spopcnt(files0 & 0xff)) * DOUBLE_PANW_PENALTY;
    value -= (spopcnt(pawns1) - spopcnt(files1 & 0xff)) * DOUBLE_PANW_PENALTY;
    isolated1 = files1 & ~(((files1 << 1) & ~FILE_H) | ((files1 >> 1) & FILE_A));
    isolated1 &= pawns1;
    isolated0 = files0 & ~(((files0 << 1) & ~FILE_H) | ((files0 >> 1) & FILE_A));
    isolated0 &= pawns0;
    value += (spopcnt(isolated0) - spopcnt(isolated1)) * ISOLATED_PAWN_PENALTY;
    return value;
}

static int passed_pawn_score_side1(uint64_t pawns1, uint64_t pawns0,
                                   uint64_t attacked, uint64_t s0)
{
    int value = spopcnt(pawns1 & RANK_7) * PAWN_RANK_7_VALUE;
    uint64_t opponent_block, passed;

    opponent_block = pawns0
                     | ((pawns0 << 1) & ~FILE_H)
                     | ((pawns0 >> 1) & ~FILE_A);
    opponent_block = kogge_stone_south(opponent_block);
    passed = pawns1 & ~opponent_block;
    value += spopcnt(passed) * PASSED_PAWN_VALUE;
    passed = kogge_stone_north(passed);
    value -= spopcnt(kogge_stone_north(passed) & attacked)
                * PASSED_PAWN_PATH_ATTACKED_PENALTY;
    value -= spopcnt(kogge_stone_north(passed) & s0)
                * PASSED_PAWN_PATH_BLOCKED_PENALTY;
    return value;
}

static int passed_pawn_score(const uint64_t bb[static 5],
                        uint64_t ranged_1, uint64_t ranged_0)
{
    int value = 0;

    value += passed_pawn_score_side1(bb_pawns_map1(bb),
                                     bb_pawns_map0(bb),
                                     ranged_0 & ~ranged_1,
                                     side0(bb));
    value -= passed_pawn_score_side1(bswap(bb_pawns_map0(bb)),
                                     bswap(bb_pawns_map1(bb)),
                                     bswap(ranged_1 & ~ranged_0),
                                     bswap(side1(bb)));
    return value;
}

static int eval_endgame(const uint64_t bb[static 5],
                        uint64_t ranged_1, uint64_t ranged_0)
{
    return passed_pawn_score(bb, ranged_1, ranged_0);
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

static int king_fortress1(const struct position *pos)
{
    return king_fortress(pawns_map1(pos),
                           rooks_only_map1(pos),
                           king_map1(pos))
        - spopcnt(pos->king_reach_map_1 & ~RANK_1 & ~pside1(pos));
}

static int king_fortress0(const struct position *pos)
{
    return king_fortress(bswap(pawns_map0(pos)),
                           bswap(rooks_only_map0(pos)),
                           bswap(king_map0(pos)))
        - spopcnt(pos->king_reach_map_0 & ~RANK_8 & ~pside0(pos));
}

static int piece_placement(const struct position *pos,
                           uint64_t outposts1, uint64_t outposts0)
{
    int value = 0;
    uint64_t pawns1 = pawns_map1(pos);
    uint64_t pawns0 = pawns_map0(pos);

    value += spopcnt(CENTER_SQ & outposts1 & knights_map1(pos))
                * KNIGHT_OUTPOST_VALUE;
    value -= spopcnt(CENTER_SQ & outposts0 & knights_map0(pos))
                * KNIGHT_OUTPOST_VALUE;
    value += spopcnt(CENTER_SQ & knights_map1(pos))
                * KNIGHT_CENTER_VALUE;
    value -= spopcnt(CENTER_SQ & knights_map0(pos))
                * KNIGHT_CENTER_VALUE;
    value += spopcnt(pawn_attacks1(pawns1) & CENTER_SQ)
                * CENTER_PAWN_ATTAK_VALUE;
    value -= spopcnt(pawn_attacks0(pawns0) & CENTER_SQ)
                * CENTER_PAWN_ATTAK_VALUE;
    value += spopcnt(pawn_attacks1(pawns1) & CENTER_SQ & (RANK_5|RANK_6))
                * CENTER_PAWN_ATTAK_VALUE;
    value -= spopcnt(pawn_attacks0(pawns0) & CENTER_SQ & (RANK_3|RANK_4))
                * CENTER_PAWN_ATTAK_VALUE;
    return value;
}

static int eval_middlegame(const struct position *pos,
                           uint64_t outposts1, uint64_t outposts0)
{
    int value = 0;

    value += king_fortress1(pos) - king_fortress0(pos);
    value += piece_placement(pos, outposts1, outposts0);
    return value;
}

static int basic_mobility(const struct position *pos,
                              uint64_t *ranged_1, uint64_t *ranged_0)
{
    uint64_t occ = occupied(pos);
    int value;

    *ranged_1 = gen_range(occ, rooks_only_map1(pos), rook_magics);
    *ranged_1 |= gen_range(occ, bishops_only_map1(pos), bishop_magics);
    *ranged_1 |= knights_attacks(knights_map1(pos));
    *ranged_0 = gen_range(occ, rooks_only_map0(pos), rook_magics);
    *ranged_0 |= gen_range(occ, bishops_only_map0(pos), bishop_magics);
    *ranged_0 |= knights_attacks(knights_map0(pos));
    value = (spopcnt(*ranged_1) - spopcnt(*ranged_0)) / 2;
    *ranged_1 |= gen_range(occ, queens_map1(pos), rook_magics);
    *ranged_1 |= gen_range(occ, queens_map1(pos), bishop_magics);
    *ranged_0 |= gen_range(occ, queens_map0(pos), rook_magics);
    *ranged_0 |= gen_range(occ, queens_map0(pos), bishop_magics);
    return value;
}

int eval(const struct position *pos)
{
    assert(pos != NULL);

    int value;
    uint64_t ranged_1, ranged_0;
    int end = compute_endgame_factor(pos->bb);

    value = eval_material(pos->bb);
    value += basic_mobility(pos, &ranged_1, &ranged_0);

    if (end > 0) {
        value += end * eval_endgame(pos->bb, ranged_1, ranged_0);
    }
    if (end < END_MAX) {
        uint64_t outposts1, outposts0;

        value += (END_MAX - end) *
                    pawn_structure(pawns_map0(pos), pawns_map1(pos),
                                        &outposts1, &outposts0);
        value += (END_MAX - end) *
                        eval_middlegame(pos, outposts1, outposts0);
    }
    
    return value;
}

eval_factors compute_eval_factors(const struct position *pos)
{
    uint64_t ranged_1, ranged_0;
    uint64_t outposts1, outposts0;
    eval_factors ef;

    ef.material = eval_material(pos->bb);
    ef.basic_mobility = basic_mobility(pos, &ranged_1, &ranged_0);
    ef.end_game = compute_endgame_factor(pos->bb);
    ef.middle_game = END_MAX - ef.end_game;
    ef.pawn_structure = pawn_structure(pawns_map0(pos), pawns_map1(pos),
                                       &outposts1, &outposts0);
    ef.passed_pawn_score = passed_pawn_score(pos->bb, ranged_1, ranged_0);
    ef.king_fortress = king_fortress1(pos) - king_fortress0(pos);
    ef.piece_placement = piece_placement(pos, outposts1, outposts0);
    return ef;
}

