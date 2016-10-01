
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8: */

#ifndef TALTOS_TALTOS_H
#define TALTOS_TALTOS_H

#include <stdbool.h>
#include <stdnoreturn.h>
#include "util.h"

#include "chess.h"
#include "game.h"
#include "book.h"

struct search_settings {
	bool use_lmr;
	bool use_null_moves;
};

struct taltos_conf {
	enum move_notation_type move_not;
	bool timing;
	taltos_systime start_time;
	unsigned main_hash_size;
	char *book_path;
	enum book_type book_type;
	bool use_unicode;
	struct search_settings search;
	const char *display_name;
};

noreturn void loop_cli(struct taltos_conf*, struct book*);

#endif
