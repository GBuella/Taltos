
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "macros.h"
#include "str_util.h"

char index_to_file_ch(int index)
{
    assert(ivalid(index));
    return (char)('h' - ind_file(index));
}

char index_to_rank_ch(int index, player turn)
{
    assert(ivalid(index));
    rank_t rank = ind_rank(index);
    return (char)((turn == black) ? ('1' + rank) : ('8' - rank));
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
        default: assert(0); return ' ';
    }
}

char square_to_char(piece p, player pl)
{
    char pc = piece_to_char(p);

    return (pl == black) ? pc : (char)toupper((unsigned char)pc);
}

piece char_to_piece(char p)
{
    switch (tolower((unsigned char)p)) {
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

file_t char_to_file(char ch)
{
    assert(is_file(ch));
    return 7 - (toupper((unsigned char)ch) - 'A');
}

rank_t char_to_rank(char ch, player turn)
{
    assert(is_rank(ch));
    return (turn == black) ? (ch - '1') : (7 - (ch - '1'));
}

int str_to_index(const char str[static 2], player turn)
{
    return ind(char_to_rank(str[1], turn), char_to_file(str[0]));
}

const char *next_token(const char *str)
{
    static const char separator[] = " \t";
    static const char line_separator[] = "\n\r";

    if (strchr(separator, *str) != NULL) {
        str += strspn(str, separator);
        if (strchr(line_separator, *str) == NULL) {
            return str;
        }
        else {
            return NULL;
        }
    }
    if (strchr(line_separator, *str) == NULL) {
        return next_token(str + strcspn(str, separator));
    }
    else {
        return NULL;
    }
}

bool empty_line(const char *line)
{
    return next_token(line) == NULL;
}

int print_nice_number(uintmax_t n,
                      const char **postfix,
                      const uintmax_t *divider)
{
    const struct lconv *lconv = localeconv();

    while (divider[1] != 0 && (*divider > n || divider[1] <= n)) {
        ++postfix;
        ++divider;
    }
    if (*divider == 1) {
        return printf("%" PRIuMAX "%s", n, *postfix);
    }
    n /= ((*divider) / 10);
    if (n % 10 == 0) {
        return printf("%" PRIuMAX "%s", n / 10, *postfix);
    }
    else {
        return printf("%" PRIuMAX "%s%" PRIuMAX "%s",
                      n / 10, lconv->decimal_point, n % 10, *postfix);
    }
}

int print_nice_count(uintmax_t n)
{
    return print_nice_number(n,
                  (const char *[]){"", "k", "m", "g", NULL},
                  (const uintmax_t[]){1, 1000, MILLION, BILLION, 0});
}

int print_nice_ns(uintmax_t n, bool use_unicode)
{
    static const char *ascii_postfixes[] = {"ns", "s", NULL};
    static const uintmax_t ascii_dividers[] = {1, BILLION, 0};

    static const char *unicode_postfixes[] = {"ns", U_MU "s", "ms", "s", NULL};
    static const uintmax_t unicode_dividers[] = {1, 1000, MILLION, BILLION, 0};

    return print_nice_number(n,
                use_unicode ? unicode_postfixes : ascii_postfixes,
                use_unicode ? unicode_dividers : ascii_dividers);
}
