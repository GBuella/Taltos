
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#include "tests.h"

#include "book.h"
#include "position.h"

#include <stdio.h>
#include <stdlib.h>

void
run_tests(void)
{
	if (prog_argc < 2)
		exit(EXIT_FAILURE);

	struct book *book = book_open(bt_polyglot, prog_argv[1]);
	if (book == NULL) {
		perror(prog_argv[1]);
		exit(EXIT_FAILURE);
	}

	struct position pos;
	enum player turn;
	int ep_index;
	move moves[MOVE_ARRAY_LENGTH];

	position_read_fen(&pos,
	    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	    &ep_index, &turn);

	book_get_move_list(book, &pos, moves);
	assert(moves[0] == create_move_pd(sq_e2, sq_e4, false));
	assert(moves[1] == 0);

	position_read_fen(&pos,
	    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
	    &ep_index, &turn);

	book_get_move_list(book, &pos, moves);
	assert(moves[0] == create_move_pd(sq_e2, sq_e4, false));
	assert(moves[1] == create_move_g(sq_h2, sq_h3, pawn, 0, false));
	assert(moves[2] == 0);

	book_close(book);
}
