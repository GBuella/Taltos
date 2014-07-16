
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "str_util.h"

char index_to_file_ch(int index)
{
    assert(ivalid(index));
    return 'h' - ind_file(index);
}

char index_to_rank_ch(int index, player turn)
{
    assert(ivalid(index));
    rank_t rank = ind_rank(index);
    return (turn == black) ? ('1' + rank) : ('8' - rank);
}

char *index_to_str(char str[static 2], int index, player turn)
{
    assert(str != NULL);
    *str++ = index_to_file_ch(index);
    *str++ = index_to_rank_ch(index, turn);
    return str;
}

char piece_to_char(piece p)
{
    switch (p) {
        case pawn: return 'p';
        case rook: return 'r';
        case knight: return 'n';
        case bishop: return 'b';
        case queen: return 'q';
        case king: return 'k';
        case 0: return ' ';
        default: assert(0);
    }
}

char square_to_char(piece p, player pl)
{
    char pc = piece_to_char(p);

    return (pl == black) ? pc : toupper(pc);
}

piece char_to_piece(char p)
{
    switch (tolower(p)) {
        case 'p': return pawn;
        case 'r': return rook;
        case 'b': return bishop;
        case 'n': return knight;
        case 'q': return queen;
        case 'k': return king;
        default: return 0;
    }
}

bool is_file(char c)
{
    return ( ((c >= 'a') && (c <= 'h'))
            || ((c >= 'A') && (c <= 'H')) );
}

bool is_rank(char c)
{
    return ((c >= '1') && (c <= '8'));
}

bool is_coordinate(const char *str)
{
    assert(str != NULL);
    return is_file(str[0]) && is_rank(str[1]);
}

unsigned char_to_file(char ch)
{
    assert(is_file(ch));
    return 7 - (toupper(ch) - 'A');
}

unsigned char_to_rank(char ch, player turn)
{
    assert(is_rank(ch));
    return (turn == black) ? (ch - '1') : (7 - (ch - '1'));
}

int str_to_index(const char str[static 2], player turn)
{
    return ind(char_to_rank(str[1], turn), char_to_file(str[0]));
}

