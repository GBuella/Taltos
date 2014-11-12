
#ifndef POLYGLOTBOOK_H
#define POLYGLOTBOOK_H

#include "chess.h"

struct polyglotbook;

struct polyglotbook *polyglotbook_open(const char *path);
void polyglotbook_close(struct polyglotbook *);
void polyglotbook_get_move(const struct polyglotbook *,
                           const struct position *,
                           size_t size,
                           move[size]);

#endif

