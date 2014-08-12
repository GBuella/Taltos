
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "macros.h"
#include "bitmanipulate.h"
#include "constants.h"
#include "chess.h"
#include "position.h"
#include "str_util.h"

static char*
print_san_move_from(const struct position *pos, move m, char *str, player turn)
{
    move moves[MOVE_ARRAY_LENGTH];
    uint64_t ambig_pieces = UINT64_C(0);
    piece p = get_piece_at(pos, mfrom(m));

    (void)gen_moves(pos, moves);
    for (move *im = moves; *im != 0; ++im) {
        if ((mfrom(*im) != mfrom(m)) && (mto(*im) == mto(m))
                && (get_piece_at(pos, mfrom(*im)) == p))
        {
            ambig_pieces |= mfrom64(*im);
        }
    }
    if ((p == pawn) && (get_piece_at(pos, mto(m)) != 0)) {
        *(str++) = index_to_file_ch(mfrom(m));
    }
    else if (nonempty(ambig_pieces)) {
        if (nonempty(ambig_pieces & file64(mfrom(m)))) {
            if (nonempty(ambig_pieces & rank64(mfrom(m)))) {
                *(str++) = index_to_file_ch(mfrom(m));
            }
            *(str++) = index_to_rank_ch(mfrom(m), turn);
        }
        else {
            *(str++) = index_to_file_ch(mfrom(m));
        }
    }
    return str;
}

static char *
print_san_move_capture(const struct position *pos, int to, char *str)
{
    if (get_piece_at(pos, to) != 0) {
        *str++ = 'x';
    }
    return str;
}

static char *print_san_promotion(move m, char *str)
{
    if (is_promotion(m)) {
        *(str++) = '=';
        *(str++) = (char)toupper(piece_to_char(mpromotion(m)));
    } 
    return str;
}

static char *print_san_check(const struct position *pos UNUSED, move m UNUSED, char *str)
{
    return str;
}

static char *
print_san_move(const struct position *pos, move m, char *str, player turn)
{
    int piece = get_piece_at(pos, mfrom(m));

    if (m == mcastle_right) {
        return str + sprintf(str, "O-O");
    }
    else if (m == mcastle_left) {
        return str + sprintf(str, "O-O-O");
    }
    if (piece != pawn) {
        *str++ = (char)toupper(piece_to_char(piece));
    }
    str = print_san_move_from(pos, m, str, turn);
    str = print_san_move_capture(pos, mto(m), str);
    str = index_to_str(str, mto(m), turn);
    str = print_san_promotion(m, str);
    str = print_san_check(pos, m, str);
    *str = '\0';
    return str;
}

char *
print_coor_move(move m, char str[static MOVE_STR_BUFFER_LENGTH], player turn)
{
    str = index_to_str(str, mfrom(m), turn);
    str = index_to_str(str, mto(m), turn);
    if (is_promotion(m)) {
        *str++ = (char)toupper(piece_to_char(mpromotion(m)));
    }
    *str = '\0';
    return str;
}

char *print_move(const struct position* pos,
                 move m,
                 char str[static MOVE_STR_BUFFER_LENGTH],
                 enum move_notation_type t,
                 player turn)
{
    assert(pos != NULL);
    assert(str != NULL);
    assert(is_move_valid(m));
    assert(is_valid_piece(get_piece_at(pos, mfrom(m))));
    assert(get_player_at(pos, mfrom(m)) != get_player_at(pos, mto(m)));

    switch (t) {
        case mn_coordinate:
            return print_coor_move(m, str, turn);
            break;
        default:
        case mn_san:
            return print_san_move(pos, m, str, turn);
            break;
    }
}

static bool move_str_eq(const char *user_move, const char *move)
{
    const char *c0 = user_move;
    const char *c1 = move;
    while (isspace(*c0)) ++c0;
    do {
        if (*c1 == '\0') {
            return (*c0 == '\0' || strchr(" \t\r\n+*-?!", *c0) != NULL);
        }
        while (!isalnum(*c0)) ++c0;
        while (!isalnum(*c1)) ++c1;
    } while (toupper(*c0++) == toupper(*c1++));
    return false;
}

int read_move(const struct position *pos, const char *str, move *m, player turn)
{
    assert(pos != NULL);
    assert(str != NULL);
    assert(m != NULL);

    move moves[MOVE_ARRAY_LENGTH];

    if (str[0] == '\0') {
        return NONE_MOVE;
    }
    (void)gen_moves(pos, moves);
    for (move *mp = moves; *mp != 0; ++mp) {
        char tstr[MOVE_STR_BUFFER_LENGTH];

        (void)print_coor_move(*mp, tstr, turn);
        if (move_str_eq(str, tstr)) {
            *m = *mp;
            return 0;
        }
        (void)print_san_move(pos, *mp, tstr, turn);
        if (move_str_eq(str, tstr)) {
            *m = *mp;
            return 0;
        }
    }
    return NONE_MOVE;
}

