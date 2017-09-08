
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
	struct {
		const char *line;
		move moves[4];
	} test_book[] = {

	{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - e4 Nf3 d4",
		{ create_move_pd(sq_e2, sq_e4),
		    create_move_g(sq_g1, sq_f3, knight, 0, false),
		    create_move_pd(sq_d2, sq_d4),
	            0, }},
	{ "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - e5 c5",
		{ create_move_pd(sq_e2, sq_e4),
		    create_move_pd(sq_c2, sq_c4),
	            0, }},
	};

	static const char path[] = "test_fen_book.txt";
	FILE *f = fopen(path, "w");
	if (f == NULL) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < ARRAY_LENGTH(test_book); ++i) {
		if (fputs(test_book[i].line, f) <= 0) {
			perror(path);
			exit(EXIT_FAILURE);
		}
		if (fputc('\n', f) == EOF) {
			perror(path);
			exit(EXIT_FAILURE);
		}
	}
	fclose(f);

	struct book *book = book_open(bt_fen, path);
	if (book == NULL) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < ARRAY_LENGTH(test_book); ++i) {
		struct position pos;
		enum player turn;
		int ep_index;
		move moves[MOVE_ARRAY_LENGTH];

		position_read_fen(&pos, test_book[i].line, &ep_index, &turn);

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
