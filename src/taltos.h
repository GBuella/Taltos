
#ifndef TALTOS_H
#define TALTOS_H

#include <stdbool.h>
#include <time.h>

#include "chess.h"
#include "game.h"
#include "book.h"

struct taltos_conf {
    enum move_notation_type move_not;
    bool timing;
    clock_t start_time;
    unsigned main_hash_size;
    unsigned aux_hash_size;
    unsigned analyze_hash_size;
    unsigned analyze_aux_hash_size;
    char *book_path;
    book_type book_type;
    bool use_unicode;
};

#if __STDC_VERSION__ >= 201112L
_Noreturn
#endif
void loop_cli(struct taltos_conf *, struct book *);

#endif
