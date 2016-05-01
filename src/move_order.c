
#include <assert.h>
#include <string.h>

#include "search.h"
#include "eval.h"
#include "timers.h"


#define USED_MOVE 0xffff

void move_fsm_setup(const struct node *node, struct move_fsm *fsm)
{
    if (is_qsearch(node)) {
        fsm->plegal_count =
            (unsigned)(gen_pcaptures(node->pos, fsm->moves) - fsm->moves);
    }
    else {
        fsm->plegal_count =
        (unsigned)(gen_plegal_moves(node->pos, fsm->moves) - fsm->moves);
    }
    fsm->legal_counter = 0;
    fsm->plegal_remaining = fsm->plegal_count;
    fsm->latest_phase = initial;
    fsm->king_b_reach = EMPTY;
    fsm->king_kn_reach = EMPTY;
    fsm->killer_i = -1;
}

static int pick_move(struct node *node, struct move_fsm *fsm, unsigned index)
{
    if (fsm->moves[index] == USED_MOVE) return -1;
    assert(fsm->plegal_remaining > 0);
    node->current_move = fsm->moves[index];
    fsm->moves[index] = USED_MOVE;
    fsm->value[index] = INT_MIN;
    fsm->plegal_remaining--;
    memcpy(node[1].pos, node->pos, sizeof node[1].pos);
    if (make_plegal_move(node[1].pos, node->current_move) != 0) {
        return 1;
    }
    node[1].is_GHI_barrier =
                 is_move_irreversible(node->pos, node->current_move);
    fsm->legal_counter++;
    fsm->index = index;
    return 0;
}

static uint64_t gen_opponent_defense(const uint64_t bb[static 5])
{
    uint64_t result;

    result = king_moves_table[bsf(bb_king_map0(bb))];
    for (uint64_t kn = bb_knights_map0(bb); nonempty(kn); kn = reset_lsb(kn)) {
        result |= knight_pattern(bsf(kn));
    }
    for (uint64_t r = bb_rooks_map0(bb); nonempty(r); r = reset_lsb(r)) {
        result |= sliding_map(bb_occ(bb), rook_magics + bsf(r));
    }
    for (uint64_t b = bb_bishops_map0(bb); nonempty(b); b = reset_lsb(b)) {
        result |= sliding_map(bb_occ(bb), bishop_magics + bsf(b));
    }
    return result;
}

static bool
is_checking(const struct position *pos, struct move_fsm *fsm, move m, piece p)
{
    if (nonempty(pos->king_reach_map_0 & mto64(m))) {
        if (is_bishop(p) || mtype(m) == pbishop) {
            if (empty(fsm->king_b_reach)) {
                fsm->king_b_reach =
                    sliding_map(occupied(pos),
                                bishop_magics + bsf(king_map0(pos)));
            }
            if (nonempty(mto64(m) & fsm->king_b_reach)) {
                return true;
            }
        }
        else if (is_rook(p) || mtype(m) == prook) {
            if (empty(fsm->king_b_reach)) {
                fsm->king_b_reach =
                    sliding_map(occupied(pos),
                                bishop_magics + bsf(king_map0(pos)));
            }
            if (empty(mto64(m) & fsm->king_b_reach)) {
                return true;
            }
        }
        else if (p == pawn) {
            if (nonempty(king_map0(pos) & (mto64(m) >> 9) & ~FILE_A)) {
                return true;
            }
            if (nonempty(king_map0(pos) & (mto64(m) >> 7) & ~FILE_H)) {
                return true;
            }
        }
    }
    else if ((mtype(m) == pknight) || mfrom64(m) & knights_map(pos)) {
        if (empty(fsm->king_kn_reach)) {
            fsm->king_kn_reach = knight_pattern(bsf(king_map0(pos)));
        }
        if (nonempty(mto64(m) & fsm->king_kn_reach)) {
            return true;
        }
    }
    return false;
}

#define PAWN_FORK_VALUE (2*PAWN_VALUE)
#define KNIGHT_FORK_VALUE (2*PAWN_VALUE)
#define CHECKING_VALUE (PAWN_VALUE + 1)

static int move_value_basic(const uint64_t board[static 5],
                            move m, piece p,
                            uint64_t opp_attack,
                            uint64_t pattack,
                            uint64_t opp_pattack)
{
    int value;
    uint64_t to64 = mto64(m);

    if (mtype(m) == en_passant) {
        value = PAWN_VALUE;
    }
    else {
        value = piece_value[bb_piece_at(board, mto(m))];
    }
    if (nonempty((opp_attack | opp_pattack) & to64)) {
        int attacker_value = piece_value[p];
        if (attacker_value > value) {
            value -= attacker_value >> 1;
        }
        else {
            value -= attacker_value >> 3;
        }
        if (empty(opp_pattack & to64) && p == knight) {
            if (nonempty(pattack && to64)) {
                if (popcnt(knight_pattern(mto(m)) & bb_rbqk0(board)) > 1) {
                    value += KNIGHT_FORK_VALUE;
                }
            }
        }
    }
    else if (mtype(m) == pqueen) {
        value += (QUEEN_VALUE - PAWN_VALUE);
    }
    else {
        switch (p) {
            case pawn:
                if (popcnt(pawn_attacks1(to64) & side0(board)) == 2) {
                    value += PAWN_FORK_VALUE;
                }
                break;
            case knight:
                if (popcnt(knight_pattern(mto(m)) & bb_rbqk0(board)) > 1) {
                    value += KNIGHT_FORK_VALUE;
                }
                break;
            default: break;
        }
    }
    return value;
}

static int
pawn_protection_value(move m, uint64_t pattack, uint64_t opp_pattack)
{
    int value = 0;
    uint64_t to64 = mto64(m);
    uint64_t from64 = mfrom64(m);

    (void) pattack;
    if (empty(opp_pattack & to64) && nonempty(opp_pattack & from64)) {
        ++value;
    }
    if (empty(opp_pattack & from64) && nonempty(opp_pattack & to64)) {
        --value;
    }
    return value;
}

static int
move_value(const struct node *node, struct move_fsm *fsm, move m,
           uint64_t opp_attack, uint64_t pattack, uint64_t opp_pattack)
{
    int value;
    piece p = get_piece_at(node->pos, mfrom(m));

    value = move_value_basic(node->pos->bb, m, p,
                             opp_attack, pattack, opp_pattack);
    if (value < -CHECKING_VALUE) {
        return value;
    }
    value += pawn_protection_value(m, pattack, opp_pattack);
    if (is_checking(node->pos, fsm, m, p)) {
        value += CHECKING_VALUE;
    }
    return value;
}

static void assign_move_values(const struct node *node, struct move_fsm *fsm)
{
    uint64_t opp_attack = gen_opponent_defense(node->pos->bb);
    uint64_t pattack = bb_pawn_attacks1(node->pos->bb);
    uint64_t opp_pattack = bb_pawn_attacks0(node->pos->bb);

    for (int i = 0; fsm->moves[i] != 0; ++i) {
        move m = fsm->moves[i];

        if (m == USED_MOVE) {
            continue;
        }
        if (m == node->killer) {
            fsm->killer_i = i;
            fsm->value[i] = INT_MIN;
        }
        else {
            fsm->value[i] = move_value(node, fsm, m,
                                   opp_attack, pattack, opp_pattack);
        }
    }
}

static int pick_killer_move(struct node *node, struct move_fsm *fsm)
{
    if (fsm->killer_i == -1) return -1;
    if (pick_move(node, fsm, fsm->killer_i) == 0) {
        return 0;
    }
    return -1;
}

static int pick_hash_move(struct node *node, struct move_fsm *fsm)
{
    if (node->hash_move_i >= 0) {
        assert(node->hash_move_i < (int)fsm->plegal_count);
        if (pick_move(node, fsm, node->hash_move_i) == 0) {
            fsm->latest_phase = hash_move;
            return 0;
        }
    }
    return -1;
}

static int
pick_next_move(struct node *node, struct move_fsm *fsm, int min_value)
{
    while (fsm->plegal_remaining > 0) {
        int best_value = min_value - 1;
        int best_index = -1;
        for (int i = 0; fsm->moves[i] != 0; ++i) {
            if (fsm->moves[i] != USED_MOVE && fsm->value[i] > best_value) {
                best_index = i;
                best_value = fsm->value[i];
            }
        }
        if (best_index != -1) {        
            if (pick_move(node, fsm, best_index) == 0) {
                return 0;
            }
        }
        else {
            return -1;
        }
    }
    return -1;
}

static int next_move_fsm(struct node *node, struct move_fsm *fsm)
{
    switch (fsm->latest_phase) {
    case initial: /* try pick first move */
        fsm->latest_phase = hash_move;
        return (pick_hash_move(node, fsm) == 0);
    case hash_move:
        assign_move_values(node, fsm);
        fsm->latest_phase = tactical_moves;
        /* fallthrough */
    case tactical_moves:
        if (pick_next_move(node, fsm, PAWN_VALUE) == 0) return 1;
        if (pick_killer_move(node, fsm) == 0) return 1;
        /* fallthrough */
    case killer:
        fsm->latest_phase = general;
        /* fallthrough */
    case general:
        if (pick_next_move(node, fsm, -PAWN_VALUE + 1) == 0) return 1;
        fsm->latest_phase = losing_moves;
        /* fallthrough */
    case losing_moves:
        if (pick_next_move(node, fsm, INT_MIN + 1) == 0) return 1;
        fsm->latest_phase = done;
        /* fallthrough */
    case done:
        return 1;
    default:
        assert(false);
    }
    return 0;
}

void select_next_move(struct node *node, struct move_fsm *fsm)
{
    timer_start(TIMER_MOVE_SELECT_NEXT);
    while (true) {
        if (fsm->plegal_remaining == 0) {
            fsm->latest_phase = done;
            break;
        }
        else if (next_move_fsm(node, fsm) != 0) {
            break;
        }
    }
    timer_stop(TIMER_MOVE_SELECT_NEXT);
}

