
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"
#include "bitmanipulate.h"
#include "chess.h"
#include "position.h"

#include "move_gen_const.inc"

struct magical rook_magics[64];
struct magical bishop_magics[64];

static move *add_moves(move *moves, int src_i, uint64_t dst_map)
{
    for (; nonempty(dst_map); dst_map = reset_lsb(dst_map)) {
        *moves++ = create_move(src_i, bsf(dst_map));
    }
    return moves;
}

static move *
gen_king_moves(const uint64_t bb[static 5], move *moves, uint64_t mask)
{
    int src_i = bsf(bb_king_map1(bb));
    int opp_ki = bsf(bb_king_map0(bb));

    uint64_t pattern = king_moves_table[src_i] & ~bb_pawn_attacks0(bb);
    pattern &= mask & ~side1(bb) & ~king_moves_table[opp_ki];
    return add_moves(moves, src_i, pattern);
}

static move *gen_castle_left(const struct position *pos, move *moves)
{
    if (pos->castle_left_1
      && empty((SQ_B1|SQ_C1|SQ_D1) & occupied(pos))
      && empty(CLEFT_PKB_THREAT_SQ & pkb_map0(pos))
      && empty(CLEFT_KNIGHT_THREAT_SQ & knights_map0(pos)))
    {
        *moves++ = mcastle_left;
    }
    return moves;
}

static move *gen_castle_right(const struct position *pos, move *moves)
{
    if (pos->castle_right_1
      && empty((SQ_F1|SQ_G1) & occupied(pos))
      && empty(CRIGHT_PKB_THREAT_SQ & pkb_map0(pos))
      && empty(CRIGHT_KNIGHT_THREAT_SQ & knights_map0(pos)))
    {
        *moves++ = mcastle_right;
    }
    return moves;
}

static move *gen_ep(const struct position *pos, move *moves)
{
    int i = pos->ep_ind;

    if (i == 0) return moves;
    if (nonempty(bit64(i + EAST) & pawns_map1(pos) & ~FILE_A)) {
        *moves++ = create_move_t(i + EAST, i + NORTH, en_passant);
    }
    if (nonempty(bit64(i + WEST) & pawns_map1(pos) & ~FILE_H)) {
        *moves++ = create_move_t(i + WEST, i + NORTH, en_passant);
    }
    return moves;
}

static move *add_pawn_moves(move *moves, int from, int to, move_type t)
{
    if (ind_rank(to) == rank_8) {
        *moves++ = create_move_t(from, to, pqueen);
        *moves++ = create_move_t(from, to, pknight);
        *moves++ = create_move_t(from, to, prook);
        *moves++ = create_move_t(from, to, pbishop);
    }
    else {
        *moves++ = create_move_t(from, to, t);
    }
    return moves;
}

static move *gen_pawn_pushes(const struct position *pos, move *moves)
{
    uint64_t pushes = (pawns_map1(pos) >> 8) & ~occupied(pos);

    for (uint64_t t = pushes; nonempty(t); t = reset_lsb(t)) {
        moves = add_pawn_moves(moves, bsf(t) + SOUTH, bsf(t), pawn_push);
    }
    pushes = (pushes >> 8) & RANK_4 & ~occupied(pos);
    for (; nonempty(pushes); pushes = reset_lsb(pushes)) {
        int i = bsf(pushes);
        *moves++ = create_move_t(i + 2*SOUTH, i, pawn_double_push);
    }
    return moves;
}

static move *
gen_pawn_captures_dir(const uint64_t bb[static 5], move *moves,
                      int dir, uint64_t mask, move_type t)
{
    uint64_t attacks = (bb_pawns_map1(bb) >> dir) & mask;

    while (nonempty(attacks)) {
        int i = bsf(attacks);

        if (t == pqueen) {
            if (ind_rank(i) == RANK_8) {
                *moves++ = create_move_t(i + dir, i, pqueen);
            }
            else {
                *moves++ = create_move_t(i + dir, i, pawn_capture);
            }
        }
        else {
            moves = add_pawn_moves(moves, i + dir, i, pawn_capture);
        }
        attacks = reset_lsb(attacks);
    }
    return moves;
}

static move *
gen_pawn_captures(const uint64_t bb[static 5], move *moves, move_type t)
{
    moves = gen_pawn_captures_dir(bb, moves, 7, side0(bb) & ~FILE_H, t);
    moves = gen_pawn_captures_dir(bb, moves, 9, side0(bb) & ~FILE_A, t);
    return moves;
}

static move *
gen_pawn_pushes_in_check(const struct position *pos, move *moves)
{
    uint64_t pushes = (pawns_map1(pos) >> 8)
                      & ~pside0(pos) & pos->king_attack_map;

    for (; nonempty(pushes); pushes = reset_lsb(pushes)) {
        int i = bsf(pushes);
        moves = add_pawn_moves(moves, i + SOUTH, i, pawn_push);
    }
    pushes = (pawns_map1(pos) >> 8) & ~occupied(pos);
    pushes = (pushes >> 8) & RANK_4 & ~pside0(pos) & pos->king_attack_map;
    for (; nonempty(pushes); pushes = reset_lsb(pushes)) {
        int i = bsf(pushes);
        *moves++ = create_move_t(i + 2*SOUTH, i, pawn_double_push);
    }
    return moves;
}

static move *
gen_pawn_captures_in_check(const struct position *pos, move *moves)
{
    moves = gen_pawn_captures_dir(pos->bb, moves, 7,
                              pside0(pos) & pos->king_attack_map & ~FILE_H, 0);
    moves = gen_pawn_captures_dir(pos->bb, moves, 9,
                              pside0(pos) & pos->king_attack_map & ~FILE_A, 0);
    return moves;
}

static move *
gen_knight_moves(const uint64_t bb[static 5], move *moves, uint64_t mask)
{
    for (uint64_t kn = bb_knights_map1(bb); nonempty(kn); kn = reset_lsb(kn)) {
        moves = add_moves(moves, bsf(kn), knight_pattern(bsf(kn)) & mask);
    }
    return moves;
}

static move *gen_sliding(const uint64_t bb[static 5],
                         move *moves,
                         uint64_t src_map,
                         const struct magical *magics,
                         uint64_t mask)
{
    uint64_t occ = bb_occ(bb);

    for (; nonempty(src_map); src_map = reset_lsb(src_map)) {
        uint64_t dst_map = sliding_map(occ, magics + bsf(src_map)) & mask;

        moves = add_moves(moves, bsf(src_map), dst_map);
    }
    return moves;
}

static move *
gen_rook_bishop_moves(const uint64_t bb[static 5], move *moves, uint64_t mask)
{
    moves = gen_sliding(bb, moves, bb_bishops_map1(bb), bishop_magics, mask);
    moves = gen_sliding(bb, moves, bb_rooks_map1(bb), rook_magics, mask);
    return moves;
}

static move *
gen_plegal_moves_in_check(const struct position *pos,
                          move moves[static MOVE_ARRAY_LENGTH])
{
    if (popcnt(pos->king_attack_map & pside0(pos)) == 1) {
        if (ind_rank(pos->ep_ind) == RANK_5) {
            uint64_t ep_mask = bit64(pos->ep_ind);
            ep_mask |= ep_mask >> 8;
            if (nonempty(pos->king_attack_map & ep_mask)) {
                moves = gen_ep(pos, moves);
            }
        }
        moves = gen_pawn_pushes_in_check(pos, moves);
        moves = gen_pawn_captures_in_check(pos, moves);
        moves = gen_knight_moves(pos->bb, moves, pos->king_attack_map);
        moves = gen_rook_bishop_moves(pos->bb, moves, pos->king_attack_map);
    }
    moves = gen_king_moves(pos->bb, moves, ~pos->king_attack_map | pside0(pos));
    return moves;
}

static move *
gen_plegal_moves_non_check(const struct position *pos,
                           move moves[static MOVE_ARRAY_LENGTH])
{
    moves = gen_ep(pos, moves);
    moves = gen_castle_left(pos, moves);
    moves = gen_castle_right(pos, moves);
    moves = gen_knight_moves(pos->bb, moves, ~pside1(pos));
    moves = gen_rook_bishop_moves(pos->bb, moves, ~pside1(pos));
    moves = gen_pawn_pushes(pos, moves);
    moves = gen_pawn_captures(pos->bb, moves, 0);
    moves = gen_king_moves(pos->bb, moves, UNIVERSE);
    return moves;
}

move *gen_plegal_moves(const struct position *pos,
                       move moves[static MOVE_ARRAY_LENGTH])
{
/* Generating pseudo-legal moves, potentially moving into / staying in check.
   Also, the castleing moves might move the king via / to squares attacked by
   opponents sliding pieces.
*/
    assert(pos != NULL);
    assert(moves != NULL);

    if (nonempty(pos->king_attack_map)) {
        moves = gen_plegal_moves_in_check(pos, moves);
    }
    else {
        moves = gen_plegal_moves_non_check(pos, moves);
    }
    *moves = 0;
    return moves;
}

move * gen_moves(const struct position *pos,
                 move moves[static MOVE_ARRAY_LENGTH])
{
    assert(pos != NULL);
    assert(moves != NULL);

    (void)gen_plegal_moves(pos, moves);
    move *src = moves;
    move *dst = moves;
    struct position t;

    while (*src != 0) {
        memcpy(&t, pos, sizeof t);
        if (make_plegal_move(&t, *src) == 0) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = 0;
    return dst;
}

static void add_sliding_king_attacks(struct position *pos,
                                     uint64_t bandits,
                                     struct magical *magics,
                                     int king_i)
{
  /*
   Add rooks/bishops attacking the king.
   Possibly more than one rook/bishop, e.g. two rooks attack
   after fxe8=Q+ in the following position:
   rnb1qk2/pppp1Ppp/5Q2/2b5/8/3P3N/PPP2PPP/RNB1KB1R w KQ - 1 5
   */
    uint64_t pattern = sliding_map(occupied(pos), magics + king_i);

    pos->king_reach_map_1 |= pattern;
    bandits &= pattern;
    pos->king_attack_map |= bandits;
    for (; nonempty(bandits); bandits = reset_lsb(bandits)) {
        pos->king_attack_map |= ray_table[king_i][bsf(bandits)];
    }
}

static void add_pawn_king_attacks(struct position *pos, uint64_t king)
{
    uint64_t pattern = ((king >> 7) & ~FILE_H);
    pattern |= ((king >> 9) & ~FILE_A);

    pos->king_attack_map |= pattern & pawns_map0(pos);
}


void gen_king_attack_map(struct position *pos)
{
    assert(pos != NULL);

    uint64_t king = king_map1(pos);

    pos->king_reach_map_1 = king;
    pos->king_attack_map = king_knight_attack(pos);
    add_pawn_king_attacks(pos, king);
    add_sliding_king_attacks(pos, rooks_map0(pos), rook_magics, bsf(king));
    add_sliding_king_attacks(pos, bishops_map0(pos), bishop_magics, bsf(king));
}

static int verify_knight_reach(struct position *pos, int king_i)
{
    uint64_t pattern = knight_pattern(king_i);

    if (nonempty(pattern & knights_map0(pos))) return -1;
    return 0;
}

static int
verify_sliding_reach(struct position *pos,
                     int king_i,
                     struct magical *magics,
                     uint64_t bandits)
{
    uint64_t pattern = sliding_map(occupied(pos), magics + king_i);

    if (nonempty(pattern & bandits)) return -1;
    pos->king_reach_map_1 |= pattern;
    return 0;
}

int process_king_zone(struct position *pos)
{
    assert(pos != NULL);

    /* Only used during make_plegal_move, to regenerate
       king_reach_map_1
       Assume king did not move to square attacked by
       opponent's pawn or king
       return -1 if king is attacked by opponent on it's
       newly occupied square */

    pos->king_reach_map_1 = king_map1(pos);

    int k = bsf(pos->king_reach_map_1);

    if (verify_knight_reach(pos, k) != 0) return -1;
    if (verify_sliding_reach(pos, k, bishop_magics, bishops_map0(pos)) != 0) {
        return -1;
    }
    if (verify_sliding_reach(pos, k, rook_magics, rooks_map0(pos)) != 0) {
        return -1;
    }
    pos->king_attack_map = EMPTY;
    return 0;
}

static void
init_sliding_move_magics(struct magical *dst,
                         const uint64_t *raw_info,
#                        ifdef SLIDING_BYTE_LOOKUP
                         const uint8_t *byte_lookup_table,
#                        endif
                         const uint64_t *table)
{
    dst->mask = raw_info[0];
    dst->multiplier = raw_info[1];
    dst->shift = (int)(raw_info[2] & 0xff);
#   ifdef SLIDING_BYTE_LOOKUP
    dst->attack_table = table + raw_info[3];
    dst->attack_index_table = byte_lookup_table + (raw_info[2]>>8);
#   else
    dst->attack_table = table + (raw_info[2]>>8);
#   endif
}

void init_move_gen(void)
{
#   ifdef SLIDING_BYTE_LOOKUP
    static const int magic_block = 4;
#   else
    static const int magic_block = 3;
#   endif
    for (int i = 0; i < 64; ++i) {
        init_sliding_move_magics(rook_magics + i,
                                 rook_magics_raw + magic_block * i,
#                                ifdef SLIDING_BYTE_LOOKUP
                                 rook_attack_index8,
#                                endif
                                 rook_magic_attacks);
        init_sliding_move_magics(bishop_magics + i,
                                 bishop_magics_raw + magic_block * i,
#                                ifdef SLIDING_BYTE_LOOKUP
                                 bishop_attack_index8,
#                                endif
                                 bishop_magic_attacks);
    }
}

move *gen_pcaptures(const struct position *pos,
                    move moves[static MOVE_ARRAY_LENGTH])
{
    assert(pos != NULL);
    assert(moves != NULL);

    /* Pseudo-legal capturing move generator,
       including missing en-passant,
       pawns are promoted only to queens */
    moves = gen_ep(pos, moves);
    moves = gen_pawn_captures(pos->bb, moves, pqueen);
    moves = gen_knight_moves(pos->bb, moves, pside0(pos));
    moves = gen_rook_bishop_moves(pos->bb, moves, pside0(pos));
    moves = gen_king_moves(pos->bb, moves, pside0(pos));
    *moves = 0;
    return moves;
}

