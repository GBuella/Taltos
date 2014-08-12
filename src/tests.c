
#undef NDEBUG

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "str_util.h"
#include "perft.h"
#include "search.h"
#include "eval.h"
#include "game.h"
#include "position.h"

static void str_test(void)
{
    assert(char_to_file('a') == file_a);
    assert(char_to_file('b') == file_b);
    assert(char_to_file('h') == file_h);
    assert(char_to_file('A') == file_a);
    assert(char_to_file('B') == file_b);
    assert(char_to_file('H') == file_h);

    assert(char_to_rank('1', white) == rank_1);
    assert(char_to_rank('2', white) == rank_2);
    assert(char_to_rank('3', white) == rank_3);
    assert(char_to_rank('4', white) == rank_4);
    assert(char_to_rank('5', white) == rank_5);
    assert(char_to_rank('8', white) == rank_8);
    assert(char_to_rank('1', black) == rank_8);
    assert(char_to_rank('2', black) == rank_7);
    assert(char_to_rank('3', black) == rank_6);
    assert(char_to_rank('4', black) == rank_5);
    assert(char_to_rank('5', black) == rank_4);
    assert(char_to_rank('8', black) == rank_1);

    assert(index_to_file_ch(0) == 'h');
    assert(index_to_file_ch(1) == 'g');
    assert(index_to_file_ch(7) == 'a');
    assert(index_to_file_ch(63) == 'a');
    
    assert(index_to_rank_ch(0, white) == '8');
    assert(index_to_rank_ch(1, white) == '8');
    assert(index_to_rank_ch(8+7, white) == '7');
    assert(index_to_rank_ch(63, white) == '1');
    assert(index_to_rank_ch(0, black) == '1');
    assert(index_to_rank_ch(1, black) == '1');
    assert(index_to_rank_ch(8+7, black) == '2');
    assert(index_to_rank_ch(63, black) == '8');

    assert(piece_to_char(queen) == 'q');

    assert(square_to_char(queen, white) == 'Q');
    assert(square_to_char(queen, black) == 'q');

    assert(is_file('f'));
    assert(is_file('F'));
    assert(!is_file('4'));
    assert(!is_file('i'));
    assert(!is_file(' '));

    assert(is_rank('1'));
    assert(is_rank('6'));
    assert(!is_rank('9'));
    assert(!is_rank('0'));
    assert(!is_rank('a'));
    assert(!is_rank(' '));

    assert(is_coordinate("g6"));
    assert(!is_coordinate("6"));
    assert(is_coordinate("g6 lorem ipsum"));
    assert(!is_coordinate("lorem ipsum"));

    assert(str_to_index("g6", white) == ind(rank_6, file_g));
    assert(str_to_index("g6", black) == flip_i(ind(rank_6, file_g)));
}

static void position_move_test(void)
{
    struct position *position;
    move move;
    char str[FEN_BUFFER_LENGTH + MOVE_STR_BUFFER_LENGTH];
    static const char *empty_fen = "8/8/8/8/8/8/8/8 w - - 0 1";
    unsigned hm, fm;
    player turn;

    position = position_create();
    if (position == NULL) abort();
    assert(position_print_fen(position, str, 1, 0, white)
            == str + strlen(empty_fen));
    assert(strcmp(empty_fen, str) == 0);
    assert(position_read_fen(position,
                             start_position_fen,
                             &fm, &hm, &turn) == 0);
    assert(hm == 0 && fm == 1 && turn == white);
    assert(position_print_fen(position, str, 1, 0, white)
            == str + strlen(start_position_fen));
    assert(strcmp(str, start_position_fen) == 0);
    move = create_move_t(str_to_index("e2", white),
                         str_to_index("e4", white),
                         pawn_double_push);
    assert(make_plegal_move(position, move) == 0);
    assert(get_piece_at(position, str_to_index("e2", black)) == nonpiece);
    assert(get_piece_at(position, str_to_index("e4", black)) == pawn);
    position_destroy(position);
}

static void game_test(void)
{
    struct game *game, *other;
    move move;

    game = game_create();
    if (game == NULL) abort();
    assert(game_turn(game) == white);
    assert(game_history_revert(game) != 0);
    assert(game_history_forward(game) != 0);
    assert(game_full_move_count(game) == 1);
    assert(game_half_move_count(game) == 0);
    move = create_move_t(ind(rank_2, file_e),
                         ind(rank_4, file_e),
                         pawn_double_push);
    assert(game_append(game, move) == 0);
    other = game_copy(game);
    if (other == NULL) abort();
    assert(game_turn(game) == black);
    assert(game_turn(other) == black);
    assert(game_history_revert(other) == 0);
    game_destroy(other);
    move = create_move_t(str_to_index("e7", black),
                         str_to_index("e5", black),
                         pawn_double_push);
    assert(game_append(game, move) == 0);
    assert(game_turn(game) == white);
    game_destroy(game);
}

void run_internal_tests(void)
{
    str_test();
    position_move_test();
    game_test();
}

