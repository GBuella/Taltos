/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
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

#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <threads>

#include "macros.h"
#include "chess.h"
#include "engine.h"
#include "transposition_table.h"
#include "taltos.h"
#include "trace.h"
#include "search.h"
#include "move_order.h"
#include "util.h"

static const char *progname;
static struct taltos::taltos_conf conf;
static void exit_routine(void);
static void usage(int status);
static void process_args(char **arg);
static void init_book(struct book **book);
static void setup_defaults(void);
static void setup_display_name(void);

const char *taltos::author_name = "Gabor Buella";
static const char *author_name_unicode = "G\U000000e1bor Buella";

int
main(int argc, char **argv)
{
	struct book *book;
	mtx_t conf_mutex;

	if (mtx_init(&conf_mutex, mtx_plain | mtx_recursive) != thrd_success)
		abort();

	conf.mutex = &conf_mutex;
	(void) argc;
	util_init();
	setup_defaults();
	trace_init(argv);
	init_zhash_table();
	init_search();
	process_args(argv);
	init_book(&book);
	init_engine(&conf);
	if (conf.search.use_history_heuristics)
		move_order_enable_history();
	else
		move_order_disable_history();

	if (atexit(exit_routine) != 0)
		return EXIT_FAILURE;
	(void) setvbuf(stdout, NULL, _IONBF, 0);

	setup_display_name();
	loop_cli(&conf, book);
}

static void
setup_defaults(void)
{
	const char *env;

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

	// Default main hash table size in megabytes
	conf.hash_table_size_mb = 32;

	conf.book_path = NULL;   // book path, none by default
	conf.book_type = bt_empty;  // use the empty book by default

	const char *e = getenv("LANG");
	if (e != NULL && strstr(e, "UTF-8") != NULL) {
		conf.use_unicode = true;
		taltos::author_name = author_name_unicode;
	}
	else {
		conf.use_unicode = false;
	}

	env = getenv("TALTOS_USE_NO_LMR");
	conf.search.use_LMR = (env == NULL || env[0] == '0');

	env = getenv("TALTOS_USE_NO_LMP");
	conf.search.use_LMP = (env == NULL || env[0] == '0');

	env = getenv("TALTOS_USE_NO_NULLM");
	conf.search.use_null_moves = (env == NULL || env[0] == '0');

	env = getenv("TALTOS_USE_PVC");
	conf.search.use_pv_cleanup = (env != NULL && env[0] != '0');

	env = getenv("TALTOS_USE_SRC");
	conf.search.use_strict_repetition_check =
	    (env != NULL && env[0] != '0');

	if (conf.search.use_strict_repetition_check) {
		conf.search.use_repetition_check = true;
	}
	else {
		env = getenv("TALTOS_USE_NORC");
		conf.search.use_repetition_check =
		    (env == NULL || env[0] == '0');
	}

	env = getenv("TALTOS_USE_HH");
	conf.search.use_history_heuristics =
	    (env != NULL && env[0] != '0');

	env = getenv("TALTOS_USE_NOBE");
	conf.search.use_beta_extensions =
	    (env == NULL || env[0] == '0');

	conf.display_name = "Taltos";
	conf.display_name_postfix = NULL;
}

static void
setup_display_name(void)
{
	static char name[0x100] = "Taltos";

	if (!conf.search.use_LMR)
		strcat(name, "-noLMR");
	if (!conf.search.use_LMP)
		strcat(name, "-noLMP");
	if (!conf.search.use_null_moves)
		strcat(name, "-nonullm");

	conf.display_name = name;
}

static void
exit_routine(void)
{
	if (conf.timing) {
		using namespace std::chrono;
		time_point end = high_resolution_clock::now();

		auto t = duration_cast<duration<uintmax_t>>(end - start_time);
		(void) fprintf(stderr, "%ju.%ju\n", t / 100, t % 100);
	}
}

static void
set_default_hash_size(const char *arg)
{
	char *endptr;
	unsigned long n = strtoul(arg, &endptr, 10);

	if (n == 0 || *endptr != '\0')
		usage(EXIT_FAILURE);

	if (!ht_is_mb_size_valid(n)) {
		(void) fprintf(stderr,
		    "Invalid hash table size \"%s\".\n"
		    "Must be a power of two, minimum %u, maximum %u.\n",
		    arg, ht_min_size_mb(), ht_max_size_mb());
		exit(EXIT_FAILURE);
	}

	conf.hash_table_size_mb = n;
}

static void
process_args(char **arg)
{
	progname = *arg;
	for (++arg; *arg != NULL && **arg != '\0'; ++arg) {
		if (strcmp(*arg, "-t") == 0) {
			conf.timing = true;
			conf.start_time = std::chrono::high_resolution_clock::now();
		}
		else if (strcmp(*arg, "--trace") == 0) {
			++arg;
			if (*arg == NULL)
				usage(EXIT_FAILURE);
		}
		else if (strcmp(*arg, "--book") == 0
		    || strcmp(*arg, "--fenbook") == 0) {
			if (arg[1] == NULL || conf.book_type != bt_empty)
				usage(EXIT_FAILURE);
			if (strcmp(*arg, "--book") == 0)
				conf.book_type = bt_polyglot;
			else
				conf.book_type = bt_fen;
			++arg;
			conf.book_path = *arg;
		}
		else if (strcmp(*arg, "--unicode") == 0) {
			conf.use_unicode = true;
			taltos::author_name = author_name_unicode;
		}
		else if (strcmp(*arg, "--help") == 0
		    || strcmp(*arg, "-h") == 0) {
			usage(EXIT_SUCCESS);
		}
		else if (strcmp(*arg, "--nolmr") == 0) {
			conf.search.use_LMR = false;
		}
		else if (strcmp(*arg, "--nolmp") == 0) {
			conf.search.use_LMP = false;
		}
		else if (strcmp(*arg, "--nonullm") == 0) {
			conf.search.use_null_moves = false;
		}
		else if (strcmp(*arg, "--SRC") == 0) {
			conf.search.use_repetition_check = true;
			conf.search.use_strict_repetition_check = true;
		}
		else if (strcmp(*arg, "--PVC") == 0) {
			conf.search.use_pv_cleanup = true;
		}
		else if (strcmp(*arg, "--AM") == 0) {
			conf.search.use_advanced_move_order = true;
		}
		else if (strcmp(*arg, "--HH") == 0) {
			conf.search.use_history_heuristics = true;
		}
		else if (strcmp(*arg, "--noBE") == 0) {
			conf.search.use_beta_extensions = false;
		}
		else if (strcmp(*arg, "--hash") == 0) {
			set_default_hash_size(*++arg);
		}
		else if (strcmp(*arg, "--name_postfix") == 0) {
			conf.display_name_postfix = *++arg;
		}
		else {
			fprintf(stderr, "Uknown option: \"%s\"\n", *arg);
			usage(EXIT_FAILURE);
		}
	}
}

static void
usage(int status)
{
	fprintf((status == EXIT_SUCCESS) ? stdout : stderr,
	    "taltos chess engine by %s\n"
	    "usage: %s [options]\n"
	    "OPTIONS:\n"
	    "  -t                  print time after quitting\n"
	    "  --trace path        log debug information to file at path\n"
	    "  --book path         load polyglot book at path\n"
	    "  --fenbook path      load FEN book at path\n"
	    "  --hash              hash table size in megabytes\n"
	    "  --unicode           use some unicode characters in the output\n"
	    "  --nolmr             do not use LMR heuristics\n"
	    "  --nolmp             do not use LMP heuristics\n"
	    "  --nonullm           do not use null move heuristics\n"
	    "  --SRC               strict repetition checking during search\n"
	    "  --AM                Use advanced move ordering - 1 ply search\n"
	    "  --HH                Use move history heuristics\n"
	    "  --PVC               PV cleanup - attempt to report cleaner PV\n"
	    "  --name_postfix      Append string to display name\n",
	    taltos::author_name, progname);
	exit(status);
}

static void
init_book(struct book **book)
{
	trace(__func__);

	errno = 0;

	*book = book_open(conf.book_type, conf.book_path);

	if (*book == NULL) {
		if (errno != 0 && conf.book_path != NULL)
			perror(conf.book_path);
		else
			fprintf(stderr, "Unable to load book %s\n",
			    conf.book_path);
		exit(EXIT_FAILURE);
	}
}
