
#ifndef GAME_H
#define GAME_H

#include "chess.h"

struct game;

struct game *game_create(void);
struct game *game_create_position(const struct position*);
struct game *game_create_fen(const char*);
struct game *game_copy(const struct game *);
char *game_print_fen(const struct game *, char[static FEN_BUFFER_LENGTH]);
void game_destroy(struct game*);
int game_append(struct game*, move);
void game_truncate(struct game*);
const struct position *game_current_position(const struct game*);
int game_history_revert(struct game*);
int game_history_forward(struct game*);
player game_turn(const struct game*);
unsigned game_full_move_count(const struct game*);
unsigned game_half_move_count(const struct game*);
bool game_is_ended(const struct game *);
move game_get_single_response(const struct game *);
bool game_has_single_response(const struct game *);

#endif
