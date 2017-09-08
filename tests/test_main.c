
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests.h"

#include "hash.h"
#include "game.h"

int prog_argc;
const char **prog_argv;
static struct game *g;

int
main(int argc, const char **argv)
{
	prog_argc = argc;
	prog_argv = argv;

	init_zhash_table();
	init_move_gen();

	run_tests();

	if (g != NULL)
		game_destroy(g);
}

const struct game*
parse_setboard_from_arg_file(void)
{
	assert(prog_argc > 1);

	char buf[1024];
	static const char command[] = "setboard";
	FILE *input;
	assert((input = fopen(prog_argv[1], "r")) != NULL);
	assert(fgets(buf, sizeof(buf), input) != NULL);
	fclose(input);
	assert(strlen(buf) > sizeof command);
	buf[strlen(command)] = '\0';
	assert(strcmp(buf, command) == 0);
	g = game_create_fen(buf + strlen(command) + 1);
	assert(g != NULL);
	return g;
}
