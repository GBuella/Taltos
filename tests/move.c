
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "tests.h"

#include "position.h"
#include "str_util.h"

#include <string.h>

void
run_tests(void)
{
	struct position *position;
	struct position *next;
	move move;
	char str[FEN_BUFFER_LENGTH + MOVE_STR_BUFFER_LENGTH];
	static const char *empty_fen = "8/8/8/8/8/8/8/8 w - - 0 1";
	unsigned hm, fm;
	int ep_index;
	enum player turn;

	position = position_allocate();
	next = position_allocate();
	assert(position_print_fen_full(position, str, 0, 1, 0, white)
	    == str + strlen(empty_fen));
	assert(strcmp(empty_fen, str) == 0);
	assert(NULL != position_read_fen_full(position, start_position_fen,
	    &ep_index, &fm, &hm, &turn));
	assert(ep_index == 0);
	assert(hm == 0 && fm == 1 && turn == white);
	assert(position_print_fen_full(position, str, 0, 1, 0, white)
	    == str + strlen(start_position_fen));
	assert(strcmp(str, start_position_fen) == 0);
	move = create_move_t(str_to_index("e2", white),
	    str_to_index("e4", white),
	    mt_pawn_double_push, pawn, 0);
	setup_registers();
	make_move(next, position, move);
	assert(position_piece_at(next, str_to_index("e2", black)) == nonpiece);
	assert(position_piece_at(next, str_to_index("e4", black)) == pawn);
	position_destroy(position);
	position_destroy(next);
}
