/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2017, Gabor Buella
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

#ifndef TALTOS_TALTOS_H
#define TALTOS_TALTOS_H

#include <stdbool.h>
#include <stdnoreturn.h>
#include <threads.h>
#include "util.h"

#include "chess.h"
#include "game.h"
#include "book.h"

struct search_settings {
	bool use_LMR; // Late Move Reductions
	bool use_LMP; // Late Move Pruning
	bool use_null_moves; // Recursive null move pruning
	bool use_pv_cleanup;
	bool use_repetition_check;
	bool use_strict_repetition_check;
	bool use_advanced_move_order;
	bool use_history_heuristics;
	bool use_beta_extensions;

	/*
	 * TODO: try these
	 *
	 * bool use_advanced_move_order;
	 * -- move ordering using full static evaluation at depth > PLY
	 *
	 * bool use_almost_mate_score;
	 * -- store in has with infinite depth, when one side has only king left
	 *
	 */
};

struct taltos_conf {
	mtx_t *mutex;
	enum move_notation_type move_not;
	bool timing;
	taltos_systime start_time;
	unsigned hash_table_size_mb;
	char *book_path;
	enum book_type book_type;
	bool use_unicode;
	struct search_settings search;
	const char *display_name;
	const char *display_name_postfix;
};

const char *author_name;

noreturn void loop_cli(struct taltos_conf*, struct book*);

#endif
