/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */
/*
 * Copyright 2017, Gabor Buella
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tests.h"

#include "book.h"
#include "position.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void
run_tests(void)
{
	if (prog_argc < 2)
		exit(EXIT_FAILURE);

	struct {
		const char *fen;
		move moves[4];
	} test_book[] = {

	{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RN1QKBNR w KQkq -",
		{ create_move_pd(sq_e2, sq_e4),
		    create_move_g(sq_g1, sq_f3, knight, 0),
		    create_move_pd(sq_d2, sq_d4),
	            0, }},
	{ "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq -",
		{ create_move_pd(sq_e2, sq_e4),
		    create_move_pd(sq_c2, sq_c4),
	            0, }},
	};

	assert(book_open(bt_fen, "/invalid_path") == NULL);

	errno = 0;
	struct book *book = book_open(bt_fen, prog_argv[1]);
	if (book == NULL) {
		perror(prog_argv[1]);
		exit(EXIT_FAILURE);
	}

	assert(book_get_size(book) == 3);

	for (size_t i = 0; i < ARRAY_LENGTH(test_book); ++i) {
		struct position pos;
		enum player turn;
		int ep_index;
		move moves[MOVE_ARRAY_LENGTH];

		position_read_fen(&pos, test_book[i].fen, &ep_index, &turn);

		book_get_move_list(book, &pos, moves);

		const move *m0 = test_book[i].moves;
		const move *m1 = moves;
		while (*m0 != 0 && *m0 == *m1) {
			++m0;
			++m1;
		}

		assert(*m0 == 0);
	}

	book_close(book);
}
