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

#include "macros.h"
#include "chess.h"
#include "engine.h"
#include "taltos.h"
#include "trace.h"
#include "search.h"
#include "move_order.h"

#include <iostream>
#include <cstring>

static const char *progname;
static taltos::taltos_conf conf;
static void exit_routine(void);
static void usage(int status);
static void process_args(char **arg);
static void setup_defaults(void);
static void setup_display_name(void);

namespace taltos
{
const char *author_name = "Gabor Buella";
}

static const char *author_name_unicode = "G\U000000e1bor Buella";

int
main(int argc, char **argv)
{
	using namespace taltos;

	(void) argc;
	setup_defaults();
	trace_init(argv);
	init_search();
	process_args(argv);
	init_engine(&conf);
	if (conf.search.use_history_heuristics)
		move_order_enable_history();
	else
		move_order_disable_history();

	if (atexit(exit_routine) != 0)
		return EXIT_FAILURE;
	(void) setvbuf(stdout, NULL, _IONBF, 0);

	setup_display_name();
	loop_cli(&conf);
}

static void
setup_defaults(void)
{
	const char *env;

	/*
	 * default move notation for printing move
	 *  it is reset to coordination notation in xboard mode
	 */
	conf.move_notation = taltos::mn_san;

	/*
	 * do not print CPU time at exit by default,
	 * can be set using the -t command line argument
	 */
	conf.timing = false;

	// Default main hash table size in megabytes
	conf.hash_table_size_mb = 32;

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
	conf.display_name_postfix = "";
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
	using namespace std::chrono;

	if (conf.timing) {
		auto t = duration_cast<milliseconds>(steady_clock::now() - conf.start_time);
		std::cerr << t.count() / 1000 << "." << t.count() % 1000 << "\n";
	}
}

static void
set_default_hash_size(const char *arg)
{
	char *endptr;
	unsigned long n = strtoul(arg, &endptr, 10);

	if (n == 0 || *endptr != '\0')
		usage(EXIT_FAILURE);

	if (n < taltos::transposition_table::min_size_mb) {
		std::cerr << "Invalid TT size " << n << "mb\n"
		    << "Minimum size is: "
		    << taltos::transposition_table::min_size_mb << "mb\n";
		exit(EXIT_FAILURE);
	}

	if (n > taltos::transposition_table::max_size_mb) {
		std::cerr << "Invalid TT size " << n << "mb\n"
		    << "Maximum size is: "
		    << taltos::transposition_table::max_size_mb << "mb\n";
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
			conf.start_time = std::chrono::steady_clock::now();
		}
		else if (strcmp(*arg, "--trace") == 0) {
			++arg;
			if (*arg == NULL)
				usage(EXIT_FAILURE);
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
