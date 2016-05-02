
#ifndef GAME_H
#define GAME_H

#include "chess.h"
#include "macros.h"

struct game;

struct game *game_create(void)
    attribute_returns_nonnull
    attribute_warn_unused_result;

struct game *game_create_position(const struct position*)
    attribute_nonnull;

struct game *game_create_fen(const char*)
    attribute_nonnull;

struct game *game_copy(const struct game*)
    attribute_nonnull
    attribute_returns_nonnull;

char *game_print_fen(const struct game*, char[static FEN_BUFFER_LENGTH])
    attribute_nonnull
    attribute_returns_nonnull;

void game_destroy(struct game*);

int game_append(struct game*, move)
    attribute_nonnull;

void game_truncate(struct game*)
    attribute_nonnull;

const struct position *game_current_position(const struct game*)
    attribute_nonnull
    attribute_returns_nonnull;

int game_history_revert(struct game*)
    attribute_nonnull;

int game_history_forward(struct game*)
    attribute_nonnull;

player game_turn(const struct game*)
    attribute_nonnull;

unsigned game_full_move_count(const struct game*)
    attribute_nonnull;

unsigned game_half_move_count(const struct game*)
    attribute_nonnull;

bool game_is_ended(const struct game*)
    attribute_nonnull;

move game_get_single_response(const struct game*)
    attribute_nonnull;

bool game_has_single_response(const struct game*)
    attribute_nonnull;

#endif
