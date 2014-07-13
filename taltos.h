
#ifndef TALTOS_H
#define TALTOS_H

#include "chess.h"
#include "game.h"

struct taltos_conf {
	enum move_notation_type move_not;
};

void loop_cli(struct taltos_conf*);

#endif
