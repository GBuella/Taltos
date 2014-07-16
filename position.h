
#ifndef POSITION_H
#define POSITION_H

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "macros.h"
#include "bitmanipulate.h"
#include "constants.h"
#include "chess.h"
#include "hash.h"

struct position {
    uint64_t bb[5];
    uint64_t king_attack_map;
    uint64_t king_reach_map_1;
    uint64_t king_reach_map_0;
    unsigned char ep_ind;
    bool castle_left_0;
    bool castle_right_0;
    bool castle_left_1;
    bool castle_right_1;
    zobrist_hash hash[2];
};

enum bitboard_index {
    bb_bit_0 = 0,
    bb_bit_1 = 1,
    bb_bit_2 = 2,
    bb_side_1 = 3,
    bb_side_0 = 4
};

static inline uint64_t side1(const uint64_t bb[static 5])
{
    return bb[bb_side_1];
}

static inline uint64_t side0(const uint64_t bb[static 5])
{
    return bb[bb_side_0];
}

static inline uint64_t pside1(const struct position *pos)
{
    return side1(pos->bb);
}

static inline uint64_t pside0(const struct position *pos)
{
    return side0(pos->bb);
}

static inline uint64_t bb_rbqk(const uint64_t bb[static 3])
{
    return bb[1];
}

static inline uint64_t bb_occ(const uint64_t bb[static 5])
{
    return bb[bb_side_1] | bb[bb_side_0];
}

static inline piece bb_piece_at_bit(const uint64_t bb[static 3], uint64_t bit)
{
    unsigned i = bsf(bit);

    return (piece)((((bb[0] & bit) >> i))
               + (((bb[1] & bit) >> i) << 1)
               + (((bb[2] & bit) >> i) << 2));
}

static inline piece bb_piece_at(const uint64_t bb[static 3], int i)
{
    assert(ivalid(i));
    return (piece)((((bb[0] >> i) & 1))
               + (((bb[1] >> i) & 1) << 1)
               + (((bb[2] >> i) & 1) << 2));
}

static inline void bb_add_piece_at(uint64_t bb[static 3], int i, piece p)
{
    assert(ivalid(i));
    assert(is_valid_piece(p));

    bb[0] |= ((uint64_t)(p & 1)) << i;
    bb[1] |= ((uint64_t)((p & 2)) >> 1) << i;
    bb[2] |= ((uint64_t)(p >> 2)) << i;
}

static inline void
bb_add_piece_at_bit(uint64_t bb[static 3], uint64_t bit, piece p)
{
    assert(is_valid_piece(p));

    bb[0] |= bit * (p & 1);
    bb[1] |= bit * ((p & 2) >> 1);
    bb[2] |= bit * (p >> 2);
}

static inline void bb_set_sq_empty(uint64_t bb[static 3], int i)
{
    assert(ivalid(i));
    uint64_t mask = ~bit64(i);

    bb[0] &= mask;
    bb[1] &= mask;
    bb[2] &= mask;
}

static inline void bb_set_piece_at(uint64_t bb[static 3], int i, piece p)
{
    assert(ivalid(i));
    assert(is_valid_piece(p));

    uint64_t mask = ~bit64(i);
    uint64_t p64 = p;
    bb[0] = (bb[0] & mask) | ((p64 & UINT64_C(1)) << i);
    bb[1] = (bb[1] & mask) | (((p64 & UINT64_C(2)) >> 1) << i);
    bb[2] = (bb[2] & mask) | ((p64 >> 2) << i);
}

static inline bool bb_player_at_bit(const uint64_t bb[static 5], uint64_t bit)
{
    return nonempty(bit & side1(bb));
}

static inline uint64_t bb_pawns_map(const uint64_t bb[static 3])
{
    return bb[0] & ~bb[1];
}

static inline uint64_t bb_pawns_map1(const uint64_t bb[static 5])
{
    return bb_pawns_map(bb) & bb[bb_side_1];
}

static inline uint64_t bb_rbqk0(const uint64_t bb[static 5])
{
    return bb_rbqk(bb) & side0(bb);
}

static inline uint64_t bb_rbqk1(const uint64_t bb[static 5])
{
    return bb_rbqk(bb) & side1(bb);
}

static inline uint64_t bb_pawns_map0(const uint64_t bb[static 5])
{
    return bb_pawns_map(bb) & bb[bb_side_0];
}

static inline uint64_t bb_knights_map(const uint64_t bb[static 3])
{
    return bb[2] & ~bb[1];
}

static inline uint64_t bb_knights_map1(const uint64_t bb[static 5])
{
    return bb_knights_map(bb) & bb[bb_side_1];
}

static inline uint64_t bb_knights_map0(const uint64_t bb[static 5])
{
    return bb_knights_map(bb) & bb[bb_side_0];
}

static inline uint64_t bb_kings_map(const uint64_t bb[static 3])
{
    return bb[1] & ~bb[0] & ~bb[2];
}

static inline uint64_t bb_king_map0(const uint64_t bb[static 5])
{
    return bb_kings_map(bb) & bb[bb_side_0];
}

static inline uint64_t bb_king_map1(const uint64_t bb[static 5])
{
    return bb_kings_map(bb) & bb[bb_side_1];
}

static inline uint64_t bb_rooks_map(const uint64_t bb[static 3])
{
    return bb[0] & bb[1];
}

static inline uint64_t bb_rooks_map1(const uint64_t bb[static 5])
{
    return bb_rooks_map(bb) & bb[bb_side_1];
}

static inline uint64_t bb_rooks_map0(const uint64_t bb[static 5])
{
    return bb_rooks_map(bb) & bb[bb_side_0];
}

static inline uint64_t bb_rooks_only_map(const uint64_t bb[static 3])
{
    return bb[0] & bb[1] & ~bb[2];
}

static inline uint64_t bb_rooks_only_map1(const uint64_t bb[static 5])
{
    return bb_rooks_only_map(bb) & bb[bb_side_1];
}

static inline uint64_t bb_rooks_only_map0(const uint64_t bb[static 5])
{
    return bb_rooks_only_map(bb) & bb[bb_side_0];
}

static inline uint64_t bb_bishops_map(const uint64_t bb[static 3])
{
    return bb[1] & bb[2];
}

static inline uint64_t bb_bishops_map1(const uint64_t bb[static 5])
{
    return bb_bishops_map(bb) & bb[bb_side_1];
}

static inline uint64_t bb_bishops_map0(const uint64_t bb[static 5])
{
    return bb_bishops_map(bb) & bb[bb_side_0];
}

static inline uint64_t bb_bishops_only_map(const uint64_t bb[static 3])
{
    return bb[1] & bb[2] & ~bb[0];
}

static inline uint64_t bb_bishops_only_map1(const uint64_t bb[static 5])
{
    return bb_bishops_only_map(bb) & bb[bb_side_1];
}

static inline uint64_t bb_bishops_only_map0(const uint64_t bb[static 5])
{
    return bb_bishops_only_map(bb) & bb[bb_side_0];
}

static inline uint64_t bb_queens_map(const uint64_t bb[static 3])
{
    return bb[0] & bb[2];
}

static inline uint64_t bb_pkb_map(const uint64_t bb[static 3])
{
    return bb[0] ^ bb[1];
}

static inline uint64_t bb_majors(const uint64_t bb[static 3])
{
    return bb[1] | bb[2];
}

static inline piece get_piece_at_bit(const struct position *pos, uint64_t bit)
{
    return bb_piece_at_bit(pos->bb, bit);
}

static inline piece get_piece_at(const struct position *pos, int i)
{
    return bb_piece_at(pos->bb, i);
}

static inline player get_player_at(const struct position *pos, int i)
{
    assert(ivalid(i));
    return nonempty(side1(pos->bb) & bit64(i));
}

static inline void add_piece_at(struct position *pos, int i, piece p)
{
    bb_add_piece_at(pos->bb, i, p);
}

static inline void add_piece_at_bit(struct position *pos, uint64_t bit, piece p)
{
    bb_add_piece_at_bit(pos->bb, bit, p);
}

static inline void set_piece_at(struct position *pos, int i, piece p)
{
    bb_set_piece_at(pos->bb, i, p);
}

static inline uint64_t rank64(int i)
{
	return RANK_8 << (i & 0x38);
}

static inline uint64_t file64(int i)
{
	return FILE_H << (i % 8);
}

static inline void set_sq_empty(struct position *pos, int i)
{
    assert(ivalid(i));
    uint64_t mask = ~bit64(i);

    pos->bb[bb_side_1] &= mask;
    pos->bb[bb_side_0] &= mask;
    bb_set_sq_empty(pos->bb, i);
}

static inline void set_sq_at(struct position *pos, int i, player pl, piece p)
{
    assert(ivalid(i));

    pos->bb[bb_side_1] &= ~bit64(i);
    pos->bb[bb_side_0] &= ~bit64(i);
    pos->bb[bb_side_1] |= (((uint64_t)pl) << i);
    pos->bb[bb_side_0] |= ((((uint64_t)pl) ^ UINT64_C(1)) << i);
    bb_set_piece_at(pos->bb, i, p);
}

static inline uint64_t pawns_map(const struct position *pos)
{
    return bb_pawns_map(pos->bb);
}

static inline uint64_t pawns_map0(const struct position *pos)
{
    return pawns_map(pos) & ~pside1(pos);
}

static inline uint64_t pawns_map1(const struct position *pos)
{
    return pside1(pos) & pawns_map(pos);
}

static inline uint64_t knights_map(const struct position *pos)
{
    return bb_knights_map(pos->bb);
}

static inline uint64_t knights_map1(const struct position *pos)
{
    return pside1(pos) & knights_map(pos);
}

static inline uint64_t knights_map0(const struct position *pos)
{
    return pside0(pos) & knights_map(pos);
}

static inline uint64_t kings_map(const struct position *pos)
{
    return bb_kings_map(pos->bb);
}

static inline uint64_t king_map1(const struct position *pos)
{
    return pside1(pos) & kings_map(pos);
}

static inline uint64_t king_map0(const struct position *pos)
{
    return pside0(pos) & kings_map(pos);
}

static inline uint64_t rooks_map(const struct position *pos)
{
    return bb_rooks_map(pos->bb);
}

static inline uint64_t rooks_map0(const struct position *pos)
{
    return pside0(pos) & rooks_map(pos);
}

static inline uint64_t rooks_map1(const struct position *pos)
{
    return pside1(pos) & rooks_map(pos);
}

static inline uint64_t rooks_only_map(const struct position *pos)
{
    return bb_rooks_only_map(pos->bb);
}

static inline uint64_t rooks_only_map0(const struct position *pos)
{
    return pside0(pos) & rooks_only_map(pos);
}

static inline uint64_t rooks_only_map1(const struct position *pos)
{
    return pside1(pos) & rooks_only_map(pos);
}

static inline uint64_t bishops_map(const struct position *pos)
{
    return bb_bishops_map(pos->bb);
}

static inline uint64_t bishops_map0(const struct position *pos)
{
    return pside0(pos) & bishops_map(pos);
}

static inline uint64_t bishops_map1(const struct position *pos)
{
    return pside1(pos) & bishops_map(pos);
}

static inline uint64_t bishops_only_map(const struct position *pos)
{
    return bb_bishops_map(pos->bb);
}

static inline uint64_t bishops_only_map1(const struct position *pos)
{
    return bishops_only_map(pos) & pside1(pos);
}

static inline uint64_t bishops_only_map0(const struct position *pos)
{
    return bishops_only_map(pos) & pside0(pos);
}

static inline uint64_t queens_map(const struct position *pos)
{
    return bb_queens_map(pos->bb);
}

static inline uint64_t queens_map1(const struct position *pos)
{
    return queens_map(pos) & pside1(pos);
}

static inline uint64_t queens_map0(const struct position *pos)
{
    return queens_map(pos) & pside0(pos);
}

static inline uint64_t occupied(const struct position *pos)
{
    return bb_occ(pos->bb);
}

static inline uint64_t pkb_map0(const struct position *pos)
{
    return bb_pkb_map(pos->bb) & pside0(pos);
}

static inline uint64_t
sliding_map(uint64_t occ, const struct magical *magic)
{
    uint64_t index = ((occ & magic->mask) * magic->multiplier) >> magic->shift;

#   ifdef SLIDING_BYTE_LOOKUP
    return magic->attack_table[magic->attack_index_table[index]];
#   else
    return magic->attack_table[index];
#   endif
}

static inline uint64_t knight_pattern(int i)
{
#	ifdef USE_KNIGHT_LOOKUP_TABLE
    return knight_moves_table[i];
#	else
    static const uint64_t pattern = UINT64_C(0x0442800000028440);
    static const uint64_t mask = UINT64_C(0x00003f3f3f3f3f3f);
    return rol(pattern, i) & (mask << ((i & 0x24)>>1));
#	endif
}

static inline uint64_t pawn_attacks0(uint64_t pawn_map)
{
    return ((pawn_map & ~FILE_H) << 7) | ((pawn_map & ~FILE_A) << 9);
}

static inline uint64_t pawn_attacks1(uint64_t pawn_map)
{
    return ((pawn_map & ~FILE_A) >> 7) | ((pawn_map & ~FILE_H) >> 9);
}

static inline uint64_t bb_pawn_attacks0(const uint64_t bb[static 5])
{
    return pawn_attacks0(bb_pawns_map0(bb));
}

static inline uint64_t bb_pawn_attacks1(const uint64_t bb[static 5])
{
    return pawn_attacks1(bb_pawns_map1(bb));
}

static inline uint64_t rook_full_attack(int i)
{
    assert(ivalid(i));
    return file64(i) | rank64(i);
}

static inline uint64_t king_knight_attack(const struct position *pos)
{
    return knight_pattern(bsf(king_map1(pos))) & knights_map0(pos);
}

static inline bool in_check(const struct position *pos)
{
    return nonempty(pos->king_attack_map);
}

static inline bool is_capture(const uint64_t bb[static 3], move m)
{
    return nonempty(mto64(m) & bb[bb_side_0]);
}

void gen_king_attack_map(struct position *);
int process_king_zone(struct position *);
void make_capture(const uint64_t[static 5], uint64_t child[static 5], move);
move *gen_captures(const uint64_t[static 5],
                   move[static MOVE_ARRAY_LENGTH],
                   uint64_t mask);
move *gen_pcaptures(const struct position *, move[static MOVE_ARRAY_LENGTH]);

#endif
