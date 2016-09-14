
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include "macros.h"
#include "chess.h"
#include "engine.h"
#include "hash.h"
#include "taltos.h"
#include "tests.h"

static const char *progname;
static bool must_run_internal_tests = true;
static struct taltos_conf conf;
static void onexit(void);
static void usage(int status);
static void process_args(char **arg);
static void init_book(struct book **book);
static void setup_defaults(void);

int
main(int argc, char **argv)
{
	struct book *book;

	(void) argc;
	setup_defaults();
	init_move_gen();
	process_args(argv);
	if (must_run_internal_tests)
		run_internal_tests();
	init_book(&book);
	init_engine(&conf);
	if (atexit(onexit) != 0)
		return EXIT_FAILURE;
	(void) setvbuf(stdout, NULL, _IOLBF, 0x1000);

	loop_cli(&conf, book);
}

static void
setup_defaults(void)
{
	/*
	 * default move notation for printing move
	 *  it is reset to coordination notation in xboard mode
	 */
	conf.move_not = mn_san;

	/*
	 * do not print CPU time at exit by default,
	 * can be set using the -t command line argument
	 */
	conf.timing = false;

	/*
	 * default main hash table size ( 2^20 buckets )
	 * - entries overwritten only with
	 * entries with greater depth
	 */
	conf.main_hash_size = 20;

	conf.book_path = NULL;   // book path, none by default
	conf.book_type = bt_builtin;  // use the builtin book by default
	conf.use_unicode = false;
}

static void
onexit(void)
{
	if (conf.timing) {
		uintmax_t t = xseconds_since(conf.start_time);
		(void) fprintf(stderr, "%ju.%ju\n", t / 100, t % 100);
	}
}

static void
process_args(char **arg)
{
	progname = *arg;
	for (++arg; *arg != NULL; ++arg) {
		if (strcmp(*arg, "-t") == 0) {
			conf.timing = true;
			conf.start_time = xnow();
		}
		else if (strcmp(*arg, "--notest") == 0) {
			must_run_internal_tests = false;
		}
		else if (strcmp(*arg, "--book") == 0
		    || strcmp(*arg, "--fenbook") == 0) {
			if (arg[1] == NULL || conf.book_type != bt_builtin)
				usage(EXIT_FAILURE);
			if (strcmp(*arg, "--book") == 0)
				conf.book_type = bt_polyglot;
			else
				conf.book_type = bt_fen;
			++arg;
			conf.book_path = *arg;
		}
		else if (strcmp(*arg, "--nobook") == 0) {
			if (conf.book_type != bt_builtin)
				usage(EXIT_FAILURE);
			conf.book_type = bt_empty;
		}
		else if (strcmp(*arg, "--unicode") == 0) {
			conf.use_unicode = true;
		}
		else if (strcmp(*arg, "--help") == 0
		    || strcmp(*arg, "-h") == 0) {
			usage(EXIT_SUCCESS);
		}
	}
}

static void
usage(int status)
{
	fprintf((status == EXIT_SUCCESS) ? stdout : stderr,
	    "taltos chess engine\n"
	    "usage: %s [options]\n"
	    "OPTIONS:"
	    "  -t                  print time after quitting\n"
	    "  --book path         load polyglot book at path\n"
	    "  --nobook            don't use any opening book\n"
	    "  --unicode           use some unicode characters in the output\n",
	    progname);
	exit(status);
}

static void
init_book(struct book **book)
{
	errno = 0;

	*book = book_open(conf.book_type, conf.book_path);

	if (*book == NULL) {
		if (errno != 0 && conf.book_path != NULL)
			perror(conf.book_path);
		exit(EXIT_FAILURE);
	}
}
