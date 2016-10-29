
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_GAME_H
#define TALTOS_GAME_H

#include "chess.h"
#include "macros.h"
#include <stddef.h>

struct game;

struct game *game_create(void)
	attribute(returns_nonnull, warn_unused_result, malloc);

struct game *game_create_position(const struct position*)
	attribute(nonnull);

struct game *game_create_fen(const char*)
	attribute(nonnull);

int game_reset_fen(struct game*, const char *fen)
	attribute(nonnull, warn_unused_result);

bool game_continues(const struct game *a, const struct game *b)
	attribute(nonnull);

struct game *game_copy(const struct game*)
	attribute(nonnull, returns_nonnull, malloc);

char *game_print_fen(const struct game*, char[static FEN_BUFFER_LENGTH])
	attribute(nonnull, returns_nonnull);

void game_destroy(struct game*);

int game_append(struct game*, move)
	attribute(nonnull);

void game_truncate(struct game*)
	attribute(nonnull);

const struct position *game_current_position(const struct game*)
	attribute(nonnull, returns_nonnull);

const struct position *game_history_position(const struct game*, int delta)
	attribute(nonnull);

int game_history_revert(struct game*)
	attribute(nonnull);

int game_history_forward(struct game*)
	attribute(nonnull);

move game_move_to_next(const struct game *g)
	attribute(nonnull);

enum player game_turn(const struct game*)
	attribute(nonnull);

unsigned game_full_move_count(const struct game*)
	attribute(nonnull);

unsigned game_half_move_count(const struct game*)
	attribute(nonnull);

bool game_is_ended(const struct game*)
	attribute(nonnull);

bool game_is_checkmate(const struct game*)
	attribute(nonnull);

bool game_is_stalemate(const struct game*)
	attribute(nonnull);

bool game_is_draw_by_insufficient_material(const struct game*)
	attribute(nonnull);

bool game_is_draw_by_repetition(const struct game*)
	attribute(nonnull);

bool game_is_draw_by_50_move_rule(const struct game*)
	attribute(nonnull);

move game_get_single_response(const struct game*)
	attribute(nonnull);

bool game_has_single_response(const struct game*)
	attribute(nonnull);

size_t game_length(const struct game*)
	attribute(nonnull);

#endif
