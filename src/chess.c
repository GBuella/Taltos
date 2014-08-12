
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "chess.h"
#include "constants.h"
#include "position.h"
#include "str_util.h"
#include "hash.h"

const char *start_position_fen = 
   "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static int coor_str_to_index(const char *str, player turn, jmp_buf jb)
{
    if (!is_file(str[0]) || !is_rank(str[1])) longjmp(jb, 1);

    return str_to_index(str, turn);
}

static char *board_print_fen(const struct position *pos, char *str)
{
    int empty_count;

    for (rank_t rank = rank_8;; rank += RSOUTH) {
        empty_count = 0;
        for (file_t file = file_a; is_valid_file(file); file += EAST) {
            piece p = get_piece_at(pos, ind(rank, file));
            if (p != nonpiece) {
                if (empty_count > 0) {
                    *str++ = '0' + (char)empty_count;
                    empty_count = 0;
                }
                *str++ = square_to_char(p, get_player_at(pos, ind(rank, file)));
            }
            else {
                ++empty_count;
            }
        }
        if (empty_count > 0) {
            *str++ = '0' + empty_count;
        }
        if (rank == rank_1) {
            return str;
        }
        else {
            *str++ = '/';
        }
    }
}

static char *castle_rights_print_fen(const struct position *pos, char *str)
{
    char *c = str;

    if (pos->castle_right_1) *c++ = 'K';
    if (pos->castle_left_1) *c++ = 'Q';
    if (pos->castle_right_0) *c++ = 'k';
    if (pos->castle_left_0) *c++ = 'q';
    if (c == str) *c++ = '-';
    return c;
}

char *position_print_fen(const struct position *pos,
                         char str[static FEN_BUFFER_LENGTH],
                         unsigned full_move,
                         unsigned half_move,
                         player turn)
{
    assert(pos != NULL);
    assert(str != NULL);

    struct position p;

    if (turn == white) {
        position_copy(&p, pos);
    }
    else {
        position_flip(&p, pos);
    }
    str = board_print_fen(&p, str);
    str += sprintf(str, " %c ", (turn == white) ? 'w' : 'b');
    str = castle_rights_print_fen(&p, str);
    *str++ = ' ';
    if (ind_rank(pos->ep_ind) != rank_5) {
        *str++ = '-';
    }
    else {
        str = index_to_str(str, pos->ep_ind + NORTH, turn);
    }
    str += sprintf(str, " %u %u", half_move, full_move);
    return str;
}

static const char *
read_pos_rank(struct position *pos, rank_t rank, const char *str, jmp_buf jb)
{
    file_t file = file_a;

    for (int i = 0; i < 8; ++str) {
        if (*str >= '1' && *str <= '8') {
            int delta = *str - '0';

            if (i+delta > 8) longjmp(jb, 1);
            i += delta;
            file += FEAST*delta;
        }
        else {
            piece p = char_to_piece(*str);

            if (!is_valid_piece(p)) longjmp(jb, 1);
            set_sq_at(pos, ind(rank, file), isupper(*str) ? white : black, p);
            file += FEAST;
            ++i;
        }
    }
    return str;
}

static const char *
read_position_squares(struct position *pos, const char *str, jmp_buf jb)
{
    rank_t rank = rank_7;

    str = read_pos_rank(pos, rank_8, str, jb);
    for (int i = 1; i < 8; ++i) {
        if (*str != '/') longjmp(jb, 1);
        str = read_pos_rank(pos, rank, str+1, jb);
        rank += RSOUTH;
    }
    if (!isspace(*str)) longjmp(jb, 1);
    return str;
}

static const char *
read_fen_turn(player *turn, const char *str, jmp_buf jb)
{
    if (strspn(str, "wWbB") != 1 || !isspace(str[1])) longjmp(jb, 1);
    *turn = (tolower(*str) == 'w') ? white : black;
    return ++str;
}

static const char *
read_castle_rights_fen(struct position *pos, const char *str, jmp_buf jb)
{
    if (str[0] == '-') {
        if (!isspace(*++str)) longjmp(jb, 1);
        return str;
    }
    for (; !isspace(*str); ++str) {
        switch (*str) {
            case 'K':
                if (pos->castle_right_1) longjmp(jb, 1);
                pos->castle_right_1 = true;
                break;
            case 'Q':
                if (pos->castle_left_1) longjmp(jb, 1);
                pos->castle_left_1 = true;
                break;
            case 'k':
                if (pos->castle_right_0) longjmp(jb, 1);
                pos->castle_right_0 = true;
                break;
            case 'q':
                if (pos->castle_left_0) longjmp(jb, 1);
                pos->castle_left_0 = true;
                break;
            default:
                longjmp(jb, 1);
        }
    }
    return str;
}

static const char *
read_ep_pos(int *ep_pos, const char *str, player turn, jmp_buf jb)
{
    if (*str == '-') {
        ++str;
    }
    else {
        if (!is_file(str[0]) || (str[1] != '6')) longjmp(jb, 1);
        *ep_pos = coor_str_to_index(str, turn, jb) + 8;
        str += 2;
    }
    if (*str != '\0' && !isspace(*str)) longjmp(jb, 1);
    return str;
}

void setup_empty_position(struct position *pos)
{
    memset(pos, 0, sizeof(*pos));
}

static const char *skip_space(const char *str)
{
    while (isspace(*str)) ++str;
    return str;
}

static const char *
read_move_counter(unsigned *n, const char *str, jmp_buf jb)
{
    char *endptr;
    unsigned long l;
    
    l = strtoul(str, &endptr, 10);
    if (l > USHRT_MAX || endptr == str) longjmp(jb, 1);
    *n = (unsigned)l;
    return endptr;
}

int position_read_fen(struct position *pos,
                      const char * volatile str,
                      unsigned *full_move,
                      unsigned *half_move,
                      player *turn)
{
    jmp_buf jb;

    if (str == NULL || pos == NULL) return -1;
    if (setjmp(jb) != 0) return -1;
    setup_empty_position(pos);
    str = read_position_squares(pos, skip_space(str), jb);
    str = read_fen_turn(turn, skip_space(str), jb);
    str = read_castle_rights_fen(pos, skip_space(str), jb);
    str = read_ep_pos(&pos->ep_ind, skip_space(str), *turn, jb);
    str = skip_space(str);
    if (*str != '\0') {
        str = read_move_counter(half_move, str, jb);
        str = skip_space(read_move_counter(full_move, skip_space(str), jb));
        if (*full_move == 0 || ((*full_move*2) < *half_move)) return -1;
        str = skip_space(str);
        if (*str != '\0') return -1;
    }
    else {
        *half_move = 0;
        *full_move = 1;
    }
    if (*turn == black) {
        gen_king_attack_map(pos);
        if (nonempty(pos->king_attack_map)) return -2;
        position_flip_ip(pos);
        gen_king_attack_map(pos);
    }
    else {
        position_flip_ip(pos);
        gen_king_attack_map(pos);
        if (nonempty(pos->king_attack_map)) return -2;
        position_flip_ip(pos);
        gen_king_attack_map(pos);
    }
    setup_zhash(pos);
    return 0;
}

void board_print(char str[static BOARD_BUFFER_LENGTH],
                 const struct position *pos,
                 player turn)
{
    assert(pos != NULL);
    assert(str != NULL);

    struct position po;
    position_copy(&po, pos);
    if (turn == black) {
        position_flip_ip(&po);
    }
    for (rank_t rank = rank_8; is_valid_rank(rank); rank += RSOUTH) {
        for (file_t file = file_a; is_valid_file(file); file += EAST) {
            piece p = get_piece_at(&po, ind(rank, file));
            player pl = get_player_at(&po, ind(rank, file));

            *str++ = square_to_char(p, pl);
        }
        *str++ = '\n';
    }
    *str++ = '\0';
}

bool is_legal_move(const struct position *pos, move m)
{
    move moves[MOVE_ARRAY_LENGTH];

    (void)gen_moves(pos, moves);
    for (unsigned i = 0; moves[i] != 0; ++i) {
        if (moves[i] == m) return true;
    }
    return false;
}

bool is_move_irreversible(const struct position *pos, move m)
{
    return (mtype(m) != general_move)
     || nonempty(pside0(pos) & mto64(m))
     || (mfrom(m) == sq_a1 && pos->castle_left_1)
     || (mfrom(m) == sq_h1 && pos->castle_right_1)
     || (mfrom(m) == sq_e1 && (pos->castle_left_1 || pos->castle_right_1))
     || (mto(m) == sq_a8 && pos->castle_left_0)
     || (mto(m) == sq_h8 && pos->castle_right_0);
}

struct position *position_create(void)
{
    return calloc(1, sizeof(struct position));
}

void position_destroy(struct position *p)
{
    free(p);
}

void position_copy(struct position *dst, const struct position *src)
{
    memcpy(dst, src, sizeof *dst);
}

void position_flip_ip(struct position *pos)
{
    pos->bb[0] = bswap(pos->bb[0]);
    pos->bb[1] = bswap(pos->bb[1]);
    pos->bb[2] = bswap(pos->bb[2]);
    uint64_t t64 = pside1(pos);
    pos->bb[bb_side_1] = bswap(pside0(pos));
    pos->bb[bb_side_0] = bswap(t64);
    pos->ep_ind = flip_i(pos->ep_ind);
    bool t = pos->castle_left_0;
    pos->castle_left_0 = pos->castle_left_1;
    pos->castle_left_1 = t;
    t = pos->castle_right_0;
    pos->castle_right_0 = pos->castle_right_1;
    pos->castle_right_1 = t;
    t64 = pos->king_reach_map_0;
    pos->king_reach_map_0 = bswap(pos->king_reach_map_1);
    pos->king_reach_map_1 = bswap(t64);
    t64 = pos->hash[1];
    pos->hash[1] = pos->hash[0];
    pos->hash[0] = t64;
}

void position_flip(struct position *dst, const struct position *src)
{
    dst->bb[0] = bswap(src->bb[0]);
    dst->bb[1] = bswap(src->bb[1]);
    dst->bb[2] = bswap(src->bb[2]);
    dst->bb[bb_side_1] = bswap(src->bb[bb_side_0]);
    dst->bb[bb_side_0] = bswap(src->bb[bb_side_1]);
    dst->castle_left_0 = src->castle_left_1;
    dst->castle_left_1 = src->castle_left_0;
    dst->castle_right_0 = src->castle_right_1;
    dst->castle_right_1 = src->castle_right_0;
    dst->king_reach_map_0 = bswap(src->king_reach_map_1);
    dst->king_reach_map_1 = bswap(src->king_reach_map_0);
    dst->hash[1] = src->hash[0];
    dst->hash[0] = src->hash[1];
    dst->king_attack_map = EMPTY;
    dst->ep_ind = 0;
}

static void remove_cl0(struct position *pos)
{
    if (pos->castle_left_0) {
        pos->castle_left_0 = false;
        z2_toggle_castle_left_0(pos->hash);
    }
}

static void remove_cr0(struct position *pos)
{
    if (pos->castle_right_0) {
        pos->castle_right_0 = false;
        z2_toggle_castle_right_0(pos->hash);
    }
}

static void remove_cl1(struct position *pos)
{
    if (pos->castle_left_1) {
        pos->castle_left_1 = false;
        z2_toggle_castle_left_1(pos->hash);
    }
}

static void remove_cr1(struct position *pos)
{
    if (pos->castle_right_1) {
        pos->castle_right_1 = false;
        z2_toggle_castle_right_1(pos->hash);
    }
}

static int make_castle_tail(struct position *pos, uint64_t rook_dst)
{
    z2_toggle_sq(pos->hash, sq_e1, king, 1);
    remove_cl1(pos);
    remove_cr1(pos);
    if (process_king_zone(pos) != 0) return -1;
    position_flip_ip(pos);
    pos->king_attack_map = EMPTY;
    if (nonempty(pos->king_reach_map_1 & rook_dst)) {
        gen_king_attack_map(pos);
    }
    return 0;
}

static void make_castle_left_pieces(struct position *pos)
{
    pos->bb[bb_side_1] ^= SQ_A1|SQ_C1|SQ_D1|SQ_E1;
    pos->bb[0] ^= SQ_A1|SQ_D1;
    pos->bb[1] ^= SQ_A1|SQ_C1|SQ_D1|SQ_E1;
    pos->ep_ind = 0;
}

static int make_castle_left(struct position *pos)
{
    make_castle_left_pieces(pos);
    z2_toggle_sq(pos->hash, sq_c1, king, 1);
    z2_toggle_sq(pos->hash, sq_a1, rook, 1);
    z2_toggle_sq(pos->hash, sq_d1, rook, 1);
    return make_castle_tail(pos, SQ_D8);
}

static void make_castle_right_pieces(struct position *pos)
{
    pos->bb[bb_side_1] ^= SQ_H1|SQ_G1|SQ_F1|SQ_E1;
    pos->bb[0] ^= SQ_H1|SQ_F1;
    pos->bb[1] ^= SQ_H1|SQ_G1|SQ_F1|SQ_E1;
    pos->ep_ind = 0;
}

static int make_castle_right(struct position *pos)
{
    make_castle_right_pieces(pos);
    z2_toggle_sq(pos->hash, sq_g1, king, 1);
    z2_toggle_sq(pos->hash, sq_h1, rook, 1);
    z2_toggle_sq(pos->hash, sq_f1, rook, 1);
    return make_castle_tail(pos, SQ_F8);
}

static void make_move_general(struct position *pos, move m)
{
    int from = mfrom(m);
    int to = mto(m);
    uint64_t mask = m64(m);

    piece p = get_piece_at(pos, mfrom(m));

    z2_toggle_sq(pos->hash, from, p, 1);
    z2_toggle_sq(pos->hash, to, p, 1);
    z2_toggle_sq(pos->hash, to, get_piece_at(pos, to), 0);
    pos->bb[bb_side_0] &= ~mask;
    pos->bb[bb_side_1] ^= mask;
    pos->bb[0] &= ~mask;
    pos->bb[1] &= ~mask;
    pos->bb[2] &= ~mask;
    add_piece_at(pos, to, p);
    switch (from) {
        case sq_a1: remove_cl1(pos); break;
        case sq_e1: remove_cl1(pos); /* fallthrough */
        case sq_h1: remove_cr1(pos); break;
    }
    switch (to) {
        case sq_a8: remove_cl0(pos); break;
        case sq_h8: remove_cr0(pos); break;
    }
    pos->ep_ind = 0;
}

static void make_promotion(struct position *pos, move m)
{
    uint64_t mask = m64(m);

    pos->bb[bb_side_1] ^= mask;
    pos->bb[bb_side_0] &= ~mask;
    pos->bb[0] &= ~mask;
    pos->bb[1] &= ~mask;
    pos->bb[2] &= ~mask;
    z2_toggle_sq(pos->hash, mfrom(m), pawn, 1);
    z2_toggle_sq(pos->hash, mto(m), get_piece_at(pos, mto(m)), 0);
    z2_toggle_sq(pos->hash, mto(m), mpromotion(m), 1);
    add_piece_at(pos, mto(m), mpromotion(m));
    switch (mto(m)) {
        case sq_a8: remove_cl0(pos); break;
        case sq_h8: remove_cr0(pos); break;
    }
    pos->ep_ind = 0;
}

static void make_pawn_capture(struct position *pos, move m)
{
    z2_toggle_sq(pos->hash, mto(m), get_piece_at(pos, mto(m)), 0);
    pos->ep_ind = 0;
    pos->bb[0] &= ~mfrom64(m);
    pos->bb[bb_side_1] ^= m64(m);
    pos->bb[0] |= mto64(m);
    pos->bb[1] &= ~mto64(m);
    pos->bb[2] &= ~mto64(m);
    pos->bb[bb_side_0] &= ~mto64(m);
    z2_toggle_sq(pos->hash, mfrom(m), pawn, 1);
    z2_toggle_sq(pos->hash, mto(m), pawn, 1);
}

static void move_pawn(struct position *pos, move m)
{
    pos->bb[0] ^= m64(m);
    pos->bb[bb_side_1] ^= m64(m);
    z2_toggle_sq(pos->hash, mfrom(m), pawn, 1);
    z2_toggle_sq(pos->hash, mto(m), pawn, 1);
}

static void make_pawn_double_push(struct position *pos, move m)
{
    move_pawn(pos, m);

    uint64_t near = ((mto64(m) >> 1) & ~FILE_A) | ((mto64(m) << 1) & ~FILE_H);

    if (nonempty(near & pawns_map0(pos))) {
        pos->ep_ind = mto(m);
        pos->hash[0] = z_toggle_ep_file(pos->hash[0], ind_file(mto(m)));
    }
    else {
        pos->ep_ind = 0;
    }
}

static void make_en_passant(struct position *pos, move m)
{
    pos->bb[bb_side_0] ^= bit64(pos->ep_ind);
    pos->bb[0] ^= bit64(pos->ep_ind);
    z2_toggle_sq(pos->hash, pos->ep_ind, pawn, 0);
    pos->ep_ind = 0;
    move_pawn(pos, m);
}

void make_move(struct position *pos, move m)
{
#   ifdef NDEBUG
    make_plegal_move(pos, m);
#   else
    assert(make_plegal_move(pos, m) ==0);
#   endif
}

static bool castle_left_ok(const struct position *pos)
{
    uint64_t occ = occupied(pos);

    /* Assuming already verified by pseaudo-legal move generator:
       squares between the queen's side rook and the king are empty,
       no opponent pawn|bishop|king on squares b2 c2 d2 e2
    */
    if (nonempty(msb(occ & FILE_D) & rooks_map0(pos))) return false;

    uint64_t attackers = msb(occ & (SQ_B2|SQ_A3))
            | msb(occ & (SQ_C2|SQ_B3|SQ_A4))
            | msb(occ & UINT64_C(0x0010080402010000))
            | msb(occ & UINT64_C(0x0008040201000000));
    if (nonempty(attackers & bishops_map0(pos))) return false;
    return true;
}

static bool castle_right_ok(const struct position *pos)
{
    uint64_t occ = occupied(pos);

    /* Assuming already verified by pseaudo-legal move generator:
       squares between the king's side rook and the king are empty,
       no opponent pawn|bishop|king on squares e2 f2 g2 h2
    */
    if (nonempty(msb(occ & FILE_F) & rooks_map0(pos))) return false;

    uint64_t attackers = msb(occ & (SQ_G2|SQ_H3))
            | msb(occ & UINT64_C(0x0004081020408000))
            | msb(occ & UINT64_C(0x0008102040800000));
    if (nonempty(attackers & bishops_map0(pos))) return false;
    return true;
}

int make_plegal_move(struct position *pos, move m)
{
    assert(pos != NULL);

    uint64_t move_mask = m64(m);
    if (ind_rank(pos->ep_ind) == RANK_5) {
        pos->hash[1] = z_toggle_ep_file(pos->hash[1], ind_file(pos->ep_ind));
    }
    if (is_promotion(m)) {
        make_promotion(pos, m);
    }
    else switch (mtype(m)) {
        case castle_left:
            if (!castle_left_ok(pos)) return -1;
            return make_castle_left(pos);
        case castle_right:
            if (!castle_right_ok(pos)) return -1;
            return make_castle_right(pos);
        case en_passant:
            assert(ivalid(pos->ep_ind));
            move_mask |= bit64(pos->ep_ind);
            make_en_passant(pos, m);
            break;
        case pawn_double_push:
            make_pawn_double_push(pos, m);
            break;
        case pawn_push:
            pos->ep_ind = 0;
            move_pawn(pos, m);
            break;
        case pawn_capture:
            make_pawn_capture(pos, m);
            break;
        default:
            make_move_general(pos, m);
            break;
    }
    if (nonempty(pos->king_reach_map_1 & move_mask)) {
        if (process_king_zone(pos) != 0) return -1;
    }
    position_flip_ip(pos);
    if (nonempty(bswap(move_mask) & pos->king_reach_map_1)) {
        gen_king_attack_map(pos);
    }
    else {
        pos->king_attack_map = king_knight_attack(pos);
    }
    assert(empty(pside1(pos) & pside0(pos)));
    assert(popcnt(king_map0(pos)) == 1);
    assert(popcnt(king_map1(pos)) == 1);
    return 0;
}

void make_capture(const uint64_t bb[static 5], uint64_t child[static 5], move m)
{
    assert(bb != NULL);
    assert(child != NULL);
    assert(mtype(m) == general_move
            || mtype(m) == pawn_capture
            || mtype(m) == pqueen);

    uint64_t from = mfrom64(m);
    uint64_t to = mto64(m);

    if (mtype(m) == pqueen) {
        child[0] = bswap((bb[0] & ~from) | to);
        child[1] = bswap(bb[1] | to);
        child[2] = bswap(bb[2] | to);
    }
    else if (mtype(m) == pawn_capture) {
        child[0] = bswap((bb[0] & ~from) | to);
        child[1] = bswap(bb[1] & ~to);
        child[2] = bswap(bb[2] & ~to);
    }
    else {
        uint64_t mask = from | to;
        child[0] = bswap(bb[0] & ~mask);
        child[1] = bswap(bb[1] & ~mask);
        child[2] = bswap(bb[2] & ~mask);
        bb_set_piece_at(child, flip_i(mto(m)), bb_piece_at(bb, mfrom(m)));
    }
    child[bb_side_0] = bswap(bb[bb_side_1] ^ (from | to));
    child[bb_side_1] = bswap(bb[bb_side_0] & ~to);
}

bool has_any_legal_move(const struct position *pos)
{
    move moves[MOVE_ARRAY_LENGTH];
    return gen_moves(pos, moves) != moves;
}

bool is_mate(const struct position *pos)
{
    if (!has_any_legal_move(pos)) {
        return nonempty(pos->king_attack_map);
    }
    return false;
}

bool is_stalemate(const struct position *pos)
{
    if (!has_any_legal_move(pos)) {
        return empty(pos->king_attack_map);
    }
    return false;
}

