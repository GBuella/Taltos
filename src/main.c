
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <threads.h>

#include "macros.h"
#include "chess.h"
#include "engine.h"
#include "hash.h"
#include "taltos.h"
#include "trace.h"
#include "search.h"
#include "move_order.h"

static const char *progname;
static struct taltos_conf conf;
static void exit_routine(void);
static void usage(int status);
static void process_args(char **arg);
static void init_book(struct book **book);
static void setup_defaults(void);
static void setup_display_name(void);

int
main(int argc, char **argv)
{
	struct book *book;
	mtx_t conf_mutex;

	if (mtx_init(&conf_mutex, mtx_plain | mtx_recursive) != thrd_success)
		abort();

	conf.mutex = &conf_mutex;
	(void) argc;
	setup_defaults();
	trace_init(argv);
	init_zhash_table();
	init_move_gen();
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
	conf.hash_table_size_mb = 256;

	conf.book_path = NULL;   // book path, none by default
	conf.book_type = bt_builtin;  // use the builtin book by default
	conf.use_unicode = false;

	env = getenv("TALTOS_USE_NO_LMR");
	conf.search.use_LMR = (env == NULL || env[0] == '0');

	env = getenv("TALTOS_USE_NO_LMP");
	conf.search.use_LMP = (env == NULL || env[0] == '0');

	env = getenv("TALTOS_USE_NO_NULLM");
	conf.search.use_null_moves = (env == NULL || env[0] == '0');

	env = getenv("TALTOS_USE_PVC");
	conf.search.use_pv_cleanup = (env != NULL && env[0] != '0');

	env = getenv("TALTOS_USE_NOSRC");
	conf.search.use_strict_repetition_check =
	    (env == NULL || env[0] == '0');

	env = getenv("TALTOS_USE_RC");
	conf.search.use_repetition_check =
	    conf.search.use_strict_repetition_check
	    || (env != NULL && env[0] != '0');

	env = getenv("TALTOS_USE_AM");
	conf.search.use_advanced_move_order =
	    (env != NULL && env[0] != '0');

	env = getenv("TALTOS_USE_NOHH");
	conf.search.use_history_heuristics =
	    (env == NULL || env[0] == '0');

	env = getenv("TALTOS_USE_NOBE");
	conf.search.use_beta_extensions =
	    (env == NULL || env[0] == '0');

	conf.display_name = "Taltos";
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
		uintmax_t t = xseconds_since(conf.start_time);
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
			conf.start_time = xnow();
		}
		else if (strcmp(*arg, "--trace") == 0) {
			++arg;
			if (*arg == NULL)
				usage(EXIT_FAILURE);
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
		else if (strcmp(*arg, "--nolmr") == 0) {
			conf.search.use_LMR = false;
		}
		else if (strcmp(*arg, "--nolmp") == 0) {
			conf.search.use_LMP = false;
		}
		else if (strcmp(*arg, "--nonullm") == 0) {
			conf.search.use_null_moves = false;
		}
		else if (strcmp(*arg, "--noSRC") == 0) {
			conf.search.use_repetition_check = false;
			conf.search.use_strict_repetition_check = false;
		}
		else if (strcmp(*arg, "--PVC") == 0) {
			conf.search.use_pv_cleanup = true;
		}
		else if (strcmp(*arg, "--AM") == 0) {
			conf.search.use_advanced_move_order = true;
		}
		else if (strcmp(*arg, "--noHH") == 0) {
			conf.search.use_history_heuristics = false;
		}
		else if (strcmp(*arg, "--noBE") == 0) {
			conf.search.use_beta_extensions = false;
		}
		else if (strcmp(*arg, "--hash") == 0) {
			set_default_hash_size(*++arg);
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
	    "taltos chess engine\n"
	    "usage: %s [options]\n"
	    "OPTIONS:\n"
	    "  -t                  print time after quitting\n"
	    "  --trace path        log debug information to file at path\n"
	    "  --book path         load polyglot book at path\n"
	    "  --hash             hash table size in megabytes\n"
	    "  --nobook            don't use any opening book\n"
	    "  --unicode           use some unicode characters in the output\n"
	    "  --nolmr             do not use LMR heuristics\n"
	    "  --nolmp             do not use LMP heuristics\n"
	    "  --nonullm           do not use null move heuristics\n"
	    "  --noSRC             strict repetition checking during search\n"
	    "  --AM                Use advanced move ordering - 1 ply search\n"
	    "  --HH                Use move history heuristics\n"
	    "  --PVC               PV cleanup - attempt to report cleaner PV\n",
	    progname);
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
		exit(EXIT_FAILURE);
	}
}
