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

#ifndef TALTOS_GAME_H
#define TALTOS_GAME_H

#include "chess.h"
#include "macros.h"
#include <stddef.h>

struct game;

struct game *game_create(void);

struct game *game_create_position(const struct position*);

struct game *game_create_fen(const char*);

int game_reset_fen(struct game*, const char *fen);

bool game_continues(const struct game *a, const struct game *b);

struct game *game_copy(const struct game*);

char *game_print_fen(const struct game*, char[static FEN_BUFFER_LENGTH]);

void game_destroy(struct game*);

int game_append(struct game*, move);

void game_truncate(struct game*);

const struct position *game_current_position(const struct game*);

const struct position *game_history_position(const struct game*, int delta);

int game_history_revert(struct game*);

int game_history_forward(struct game*);

move game_move_to_next(const struct game *g);

enum player game_turn(const struct game*);

unsigned game_full_move_count(const struct game*);

unsigned game_half_move_count(const struct game*);

bool game_is_ended(const struct game*);

bool game_is_checkmate(const struct game*);

bool game_is_stalemate(const struct game*);

bool game_is_draw_by_insufficient_material(const struct game*);

bool game_is_draw_by_repetition(const struct game*);

bool game_is_draw_by_50_move_rule(const struct game*);

move game_get_single_response(const struct game*);

bool game_has_single_response(const struct game*);

size_t game_length(const struct game*);

#endif
