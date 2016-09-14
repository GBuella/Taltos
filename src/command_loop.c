
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "book.h"
#include "perft.h"
#include "game.h"
#include "engine.h"
#include "taltos.h"
#include "hash.h"
#include "eval.h"
#include "str_util.h"

struct cmd_entry {
	const char text[16];
	void (*cmd_func)(void);
	const char *paramstr;
};

static struct taltos_conf *conf;
static struct book *book;
static jmp_buf command_error;
static int error_condition = 0;

#define CMD_PARAM_ERROR 1
#define CMD_GENERAL_ERROR 2

#define INTERNAL_ERROR() { \
	(void) fprintf(stderr, "Internal error in %s\n", __func__); \
	exit(EXIT_FAILURE); }

static void
param_error(void)
{
	error_condition = CMD_PARAM_ERROR;
	longjmp(command_error, 1);
}

static void
general_error(void)
{
	error_condition = CMD_GENERAL_ERROR;
	longjmp(command_error, 1);
}

static const char *features[] = {
	"ping=1",
	"setboard=1",
	"sigint=1",
	"reuse=1",
	"myname=\"Taltos\"",
	NULL
};

static bool is_force_mode;
static enum player computer_side;
static bool is_xboard;
static bool can_ponder;
static const char *whose_turn[] = {"whites", "blacks"};
static bool game_started;
static bool exit_on_done;
static bool verbose;

static struct game *game = NULL;

static const struct position*
current_position(void)
{
	return game_current_position(game);
}

static void
add_move(move m)
{
	if (game_append(game, m) != 0)
		INTERNAL_ERROR();
	engine_process_move(m);
}

static int
revert(void)
{
	return game_history_revert(game);
}

static int
forward(void)
{
	return game_history_forward(game);
}

static enum player
turn(void)
{
	return game_turn(game);
}

static bool
is_comp_turn(void)
{
	return turn() == computer_side;
}

static bool
is_opp_turn(void)
{
	return !is_comp_turn();
}

static bool
is_end(void)
{
	return game_is_ended(game);
}

static bool
has_single_response(void)
{
	return game_has_single_response(game);
}

static move
get_single_response(void)
{
	return game_get_single_response(game);
}

static void dispatch_command(const char *cmd);

static void
set_xboard(void)
{
	is_xboard = true;
	puts("");
#if !defined(WIN32) && !defined(WIN64)
	(void) signal(SIGINT, SIG_IGN);
#endif
}

static char*
get_str_arg_opt(void)
{
	return strtok(NULL, " \t\n\r");
}

static char*
get_str_arg(void)
{
	char *str;

	if ((str = get_str_arg_opt()) == NULL) {
		param_error();
	}
	return str;
}

static long
get_long_arg(void)
{
	long n;
	char *str;
	char *endptr;

	str = get_str_arg();
	n = strtol(str, &endptr, 0);
	if (n == LONG_MAX || n == LONG_MIN || *endptr != '\0') {
		(void) fprintf(stderr, "Invalid numeric argument\n");
		param_error();
	}
	return n;
}

static char*
str_to_lower(char *str)
{
	for (char *c = str; *c != '\0'; ++c)
		*c = (char)tolower((unsigned char)*c);
	return str;
}

static char*
get_str_arg_lower_opt(void)
{
	char *str;

	if ((str = get_str_arg_opt()) != NULL)
		(void) str_to_lower(str);
	return str;
}

static char*
get_str_arg_lower(void)
{
	return str_to_lower(get_str_arg());
}

static void
print_computer_move(move m)
{
	char str[MOVE_STR_BUFFER_LENGTH];
	enum move_notation_type mn;

	if (is_xboard)
		mn = mn_coordinate;
	else
		mn = conf->move_not;
	(void) print_move(current_position(), m, str, mn, turn());
	if (is_xboard) {
		printf("move %s\n", str);
	}
	else {
		printf("%u. ", game_full_move_count(game));
		if (turn() == black) {
			printf("... ");
		}
		printf("%s\n", str);
	}
}

static void
decide_move(void)
{
	if (!is_end()) {
		if (is_force_mode || !game_started) {
			return;
		}
		if (game_started && has_single_response()) {
			move m = get_single_response();

			print_computer_move(m);
			add_move(m);
			engine_move_count_inc();
		}
		else {
			move m = book_get_move(book, current_position());
			if (m != none_move) {
				print_computer_move(m);
				add_move(m);
				engine_move_count_inc();
			}
			else {
				start_thinking();
			}
		}
	}
	else {
		game_started = false;
	}
}

static void
operator_move(move m)
{
	stop_thinking();
	if (!is_force_mode)
		game_started = true;
	add_move(m);
	decide_move();
}

static void
print_move_path(const struct game *original_game,
		const move *m,
		enum move_notation_type mn)
{
	char str[MOVE_STR_BUFFER_LENGTH];
	bool first = true;
	struct game *g = game_copy(original_game);

	if (g == NULL)
		INTERNAL_ERROR();
	for (; *m != 0; ++m) {
		if (game_turn(g) == white || first)
			printf("%u. ", game_full_move_count(g));
		if (first && game_turn(g) == black)
			printf("... ");
		first = false;
		(void) print_move(game_current_position(g),
		    *m, str, mn, game_turn(g));
		printf("%s ", str);
		if (game_append(g, *m) != 0)
			INTERNAL_ERROR();
	}
	game_destroy(g);
}

static void
print_percent(int p)
{
	if (p == -1) {
		putchar('-');
	}
	else {
		if (p < 100)
			putchar(' ');
		printf("%d.%d%%", p / 10, p % 10);
	}
}

static void
print_result_header(void)
{
	if (verbose)
		printf("  D\tQD\ttime\tvalue\tfmc\thuse\tnodes\tPV\n");
	else
		printf("  D\ttime\tvalue\tnodes\tPV\n");
}

static void
print_verbose_search_info(struct engine_result res)
{
	struct search_result sres = res.sresult;
	if (sres.cutoff_count > 0)
		print_percent((sres.first_move_cutoff_count * 1000)
		    / sres.cutoff_count);
	else
		print_percent(0);
	putchar('\t');
	print_percent(res.ht_usage);
	putchar('\t');
}

static void
print_depth(struct engine_result res)
{
	if (res.sresult.selective_depth > 0)
		printf("%u/%u", res.depth, res.sresult.selective_depth);
	else
		printf("%u/0", res.depth);
	if (verbose)
		printf("\t%d",
		    (res.sresult.qdepth > 0) ? res.sresult.qdepth : 0);
}

static void
print_centipawns(int value)
{
	if (value < 0)
		putchar('-');
	printf("%d.%.2d", abs(value) / 100, abs(value) % 100);
}

static void
print_current_result(struct engine_result res)
{
	if (is_xboard) {
		printf("%u %d %ju %ju ",
		    res.depth, res.sresult.value,
		    res.time_spent, res.sresult.node_count);
	}
	else {
		if (res.first)
			print_result_header();
		putchar(' ');
		print_depth(res);
		putchar('\t');
		printf("%ju.%.2ju", res.time_spent / 100, res.time_spent % 100);
		putchar('\t');
		print_centipawns(res.sresult.value);
		putchar('\t');
		if (verbose)
			print_verbose_search_info(res);
		(void) print_nice_count(res.sresult.node_count);
		printf("N\t");
	}
	print_move_path(game, res.pv, conf->move_not);
	putchar('\n');
}

static void
computer_move(void)
{
	move m;

	if (engine_get_best_move(&m) != 0) {
		puts("-");
		return;
	}
	print_computer_move(m);
	if (exit_on_done)
		exit(EXIT_SUCCESS);
	add_move(m);
	engine_move_count_inc();
}

static int
try_read_move(const char *cmd)
{
	move move;

	switch (read_move(current_position(), cmd, &move, turn())) {
		case none_move:
			return 1;
		case illegal_move:
			printf("Illegal move: %s\n", cmd);
			return 0;
		case 0:
			if (!is_force_mode && !is_opp_turn()) {
				printf("It is not %s's turn\n",
				    whose_turn[opponent_of(computer_side)]);
				return 0;
			}
			operator_move(move);
			return 0;
		default: assert(0);
	}
	return 0;
}

static void init_settings(void);

void
loop_cli(struct taltos_conf *arg_conf, struct book *arg_book)
{
	char line[1024];
	char *cmd;

	assert(arg_conf != NULL && arg_book != NULL);
	conf = arg_conf;
	book = arg_book;
	init_settings();
	while (true) {
		if (fgets(line, sizeof line, stdin) == NULL) {
			if (exit_on_done)
				wait_thinking();
			exit(EXIT_SUCCESS);
		}
		if ((cmd = strtok(line, " \t\n")) == NULL)
			continue;
		if (try_read_move(cmd) != 0)
			dispatch_command(cmd);
	}
}

static void
cmd_quit(void)
{
	stop_thinking();
	exit(EXIT_SUCCESS);
}

static int
get_int(int min, int max)
{
	long n;

	n = get_long_arg();
	if (n < (long)min) {
		(void) fprintf(stderr, "Number too low: %ld\n", n);
		param_error();
	}
	if (n > (long)max) {
		(void) fprintf(stderr, "Number too high: %ld\n", n);
		param_error();
	}
	return (int)n;
}

static unsigned
get_uint(unsigned min, unsigned max)
{
	long n;

	n = get_long_arg();
	if (n < (long)min) {
		(void) fprintf(stderr, "Number too low: %ld\n", n);
		param_error();
	}
	if (n > (long)max) {
		(void) fprintf(stderr, "Number too high: %ld\n", n);
		param_error();
	}
	return (unsigned)n;
}

static void
cmd_perft(void)
{
	printf("%lu\n", perft(current_position(), get_uint(1, 1024)));
}

static void
cmd_perfto(void)
{
	printf("%lu\n", perft_ordered(current_position(), get_uint(1, 1024)));
}

static void
cmd_qperft(void)
{
	printf("%lu\n", qperft(current_position(), get_uint(1, 1024)));
}

static void
cmd_perfts(void)
{
	unsigned depth;

	depth = get_uint(1, 1024);
	for (unsigned i = 1; i <= depth; ++i) {
		unsigned long n = perft(current_position(), i);
		printf("%s%u : %ld\n", (i < 10) ? " " : "", i, n);
	}
}

static void
run_cmd_divide(bool ordered)
{
	struct divide_info *dinfo;
	const char *line;

	dinfo = divide_init(current_position(),
	    get_uint(0, 1024),
	    turn(),
	    ordered);
	while ((line = divide(dinfo, conf->move_not)) != NULL)
		puts(line);
	divide_destruct(dinfo);
}

static void
cmd_divide(void)
{
	run_cmd_divide(false);
}

static void
cmd_divideo(void)
{
	run_cmd_divide(true);
}

static void
cmd_setboard(void)
{
	struct game *g;

	if (game_started)
		return;
	if ((g = game_create_fen(strtok(NULL, "\n\r"))) == NULL) {
		(void) fprintf(stderr, "Unable to parse FEN\n");
		return;
	}
	game_destroy(game);
	game = g;
	reset_engine(current_position());
}

static void
cmd_printboard(void)
{
	char str[BOARD_BUFFER_LENGTH];

	(void) board_print(str, current_position(), turn());
	(void) fputs(str, stdout);
}

static void
cmd_printfen(void)
{
	char str[FEN_BUFFER_LENGTH];

	(void) game_print_fen(game, str);
	puts(str);
}

static void
cmd_echo(void)
{
	char *str;

	if ((str = strtok(NULL, "\n\r")) != NULL)
		puts(str);
}

static void
cmd_new(void)
{
	stop_thinking();
	game_destroy(game);
	if ((game = game_create()) == NULL)
		INTERNAL_ERROR();
	reset_engine(current_position());
	computer_side = black;
	set_thinking_done_cb(computer_move);
	unset_search_depth_limit();
	if (!is_xboard)
		puts("New game - computer black");
	game_started = false;
	is_force_mode = false;
}

static void
cmd_sd(void)
{
	set_search_depth_limit(get_uint(0, MAX_PLY));
}

static void
cmd_nps(void)
{
	set_search_nps(get_uint(0, UINT_MAX));
}

static void
cmd_hint(void)
{
	move m;
	char str[MOVE_STR_BUFFER_LENGTH];

	if (engine_get_best_move(&m) == 0) {
		print_move(current_position(), m, str, conf->move_not, turn());
		printf("Hint: %s\n", str);
	}
}

static void
cmd_hard(void)
{
	can_ponder = true;
	decide_move();
}

static void
cmd_easy(void)
{
	can_ponder = false;
	if (is_comp_turn())
		stop_thinking();
}

static void
cmd_result(void)
{
	stop_thinking();
	game_started = false;
}

static void
cmd_time(void)
{
	set_computer_clock(get_uint(0, UINT_MAX));
}

static void
cmd_otim(void)
{
	set_opponent_clock(get_uint(0, UINT_MAX));
}

static void
cmd_black(void)
{
	computer_side = black;
	if (is_comp_turn() && !is_force_mode && game_started)
		decide_move();
}

static void
cmd_white(void)
{
	computer_side = white;
	if (is_comp_turn() && !is_force_mode && game_started)
		decide_move();
}

static void
cmd_level(void)
{
	unsigned mps, base, inc;

	mps = get_uint(0, 1024);
	base = get_uint(0, 1024);
	inc = get_uint(0, 1024);
	set_moves_left_in_time(mps);
	set_computer_clock(base * 100);
	set_time_inc(inc * 100);
}

static void
cmd_protover(void)
{
	printf("feature");
	for (const char **f = features; *f != NULL; ++f)
		printf(" %s", *f);
	printf("\nfeature done=1\n");
}

static void
cmd_force(void)
{
	stop_thinking();
	is_force_mode = true;
}

static void
cmd_go(void)
{
	if (game_started && is_comp_turn())
		return;
	if (!is_comp_turn())
		computer_side = opponent_of(computer_side);
	is_force_mode = false;
	game_started = true;
	decide_move();
}

static void
cmd_playother(void)
{
	if (!game_started || !is_force_mode || is_comp_turn())
		return;
	stop_thinking();
	computer_side = opponent_of(computer_side);
	decide_move();
}

static void
cmd_st(void)
{
	set_secs_per_move(get_uint(1, 0x10000));
}

static void
set_var_onoff(bool *variable)
{
	char *str;

	if ((str = get_str_arg_lower_opt()) == NULL) {
		*variable = true;
		return;
	}
	if (strcmp(str, "on") == 0) {
		*variable = true;
	}
	else if (strcmp(str, "off") == 0) {
		*variable = false;
	}
	else {
		param_error();
	}
}

static void
set_verbosity(void)
{
	set_var_onoff(&verbose);
}

static void
set_exitondone(void)
{
	set_var_onoff(&exit_on_done);
}

static void
search_cb(void)
{
	move m;

	if (engine_get_best_move(&m) != 0) {
		// error?
		return;
	}
	print_computer_move(m);
	set_thinking_done_cb(computer_move);
	if (exit_on_done)
		exit(EXIT_SUCCESS);
}

static void
cmd_search(void)
{
	if (game_started)
		return;
	set_thinking_done_cb(search_cb);
	start_thinking_no_time_limit();
}

static void
cmd_search_sync(void)
{
	if (game_started)
		return;
	set_thinking_done_cb(search_cb);
	start_thinking_no_time_limit();
	wait_thinking();
}

static void
cmd_analyze(void)
{
	/*
	 * TODO
	 * if (game_started) {
	 *  return;
	 * }
	 * set_thinking_done_cb(search_cb);
	 * start_analyze();
	 */
}

static void
cmd_undo(void)
{
	if (is_force_mode) {
		if (revert() != 0)
			general_error();
		reset_engine(current_position());
	}
}

static void
cmd_redo(void)
{
	if (is_force_mode) {
		if (forward() != 0)
			general_error();
		reset_engine(current_position());
	}
}

static void
cmd_setmovenot(void)
{
	char *str = get_str_arg_lower();

	if (strcmp(str, "coor") == 0)
		conf->move_not = mn_coordinate;
	else if (strcmp(str, "san") == 0)
		conf->move_not = mn_san;
	else
		param_error();
}

static void
cmd_getmovenot(void)
{
	switch (conf->move_not) {
		case mn_coordinate:
			puts("Using Coordinate move notation");
			break;
		case mn_san:
			puts("Using Standard algebraic notation");
			break;
		default:
			break;
	}
}

static void
cmd_set_show_thinking(void)
{
	set_show_thinking(print_current_result);
}

static void
cmd_ping(void)
{
	char *str;

	if ((str = get_str_arg_opt()) == NULL)
		puts("pong");
	else
		printf("pong %s\n", str);
}

static void
cmd_eval(void)
{
	struct eval_factors ef = compute_eval_factors(current_position());
	printf(" material:            ");
	print_centipawns(ef.material);
	printf("\n middle_game factor:  %d", ef.middle_game);
	printf("\n end_game factor:     %d", ef.end_game);
	printf("\n basic_mobility:      ");
	print_centipawns(ef.basic_mobility);
	printf("\n center_control:      ");
	print_centipawns(ef.center_control);
	printf("\n pawn_structure:      ");
	print_centipawns(ef.pawn_structure);
	printf("\n passed_pawns:        ");
	print_centipawns(ef.passed_pawns);
	printf("\n king_fortress:       ");
	print_centipawns(ef.king_fortress);
	printf("\n rook_placement:      ");
	print_centipawns(ef.rook_placement);
	printf("\n knight_placement:    ");
	print_centipawns(ef.knight_placement);
	printf("\n value:               ");
	print_centipawns(eval(current_position()));
	putchar('\n');
}

static void
cmd_poskey(void)
{
	uint64_t key[2];
	get_position_key(current_position(), key);
	printf("%016" PRIx64 " %016" PRIx64 "\n", key[0], key[1]);
}

static void
cmd_polyglotkey(void)
{
	printf("%016" PRIx64 "\n",
	    position_polyglot_key(current_position(), turn()));
}

static void
cmd_hash_value(void)
{
	ht_entry entry = engine_current_entry();
	if (!ht_is_set(entry)) {
		puts("hash_value: none");
		return;
	}
	printf("hash_value: ");
	if (ht_value_type(entry) == vt_none) {
		puts("none");
		return;
	}
	if (ht_value_type(entry) == vt_exact)
		printf("exact ");
	else if (ht_value_type(entry) == vt_upper_bound)
		printf("upper bound ");
	else
		printf("upper bound ");
	print_centipawns(ht_value(entry));
	putchar('\n');
}

static void
cmd_hash_value_exact_min(void)
{
	int minimum = get_int(-mate_value, mate_value);

	ht_entry entry = engine_current_entry();
	if (ht_is_set(entry) && ht_value_type(entry) == vt_exact
	    && ht_value(entry) >= minimum)
		puts("ok");
	else
		puts("no");
}

static void
cmd_hash_value_exact_max(void)
{
	int maximum = get_int(-mate_value, mate_value);

	ht_entry entry = engine_current_entry();
	if (ht_is_set(entry) && ht_value_type(entry) == vt_exact
	    && ht_value(entry) <= maximum)
		puts("ok");
	else
		puts("no");
}

static void
cmd_position(void)
{
	puts("board:");
	cmd_printboard();
	printf("\nFEN: ");
	cmd_printfen();
	printf("internal hash key: ");
	cmd_poskey();
	printf("polyglot hash key: ");
	cmd_polyglotkey();
	puts("static evaluation:");
	cmd_eval();
}

static void
cmd_getpv(void)
{
}

static void nop(void) {}

static struct cmd_entry cmd_list[] = {
/* BEGIN CSTYLED */
	{"q",            cmd_quit,               NULL},
	{"quit",         cmd_quit,               NULL},
	{"exit",         cmd_quit,               NULL},
	{"perft",        cmd_perft,              NULL},
	{"perfto",       cmd_perfto,             NULL},
	{"qperft",       cmd_qperft,             NULL},
	{"perfts",       cmd_perfts,             NULL},
	{"divide",       cmd_divide,             "depth"},
	{"divideo",      cmd_divideo,            "depth"},
	{"setboard",     cmd_setboard,           "FENSTRING"},
	{"printboard",   cmd_printboard,         NULL},
	{"printfen",     cmd_printfen,           NULL},
	{"echo",         cmd_echo,               NULL},
	{"print",        cmd_echo,               NULL},
	{"getpv",        cmd_getpv,              NULL},
	{"xboard",       set_xboard,             NULL},
	{"new",          cmd_new,                NULL},
	{"protover",     cmd_protover,           NULL},
	{"time",         cmd_time,               NULL},
	{"force",        cmd_force,              NULL},
	{"otim",         cmd_otim,               NULL},
	{"sd",           cmd_sd,                 NULL},
	{"nps",          cmd_nps,                NULL},
	{"go",           cmd_go,                 NULL},
	{"result",       cmd_result,             NULL},
	{"hint",         cmd_hint,               NULL},
	{"hard",         cmd_hard,               NULL},
	{"easy",         cmd_easy,               NULL},
	{"post",         cmd_set_show_thinking,  NULL},
	{"nopost",       set_no_show_thinking,   NULL},
	{"level",        cmd_level,              NULL},
	{"black",        cmd_black,              NULL},
	{"white",        cmd_white,              NULL},
	{"playother",    cmd_playother,          NULL},
	{"st",           cmd_st,                 NULL},
	{"accepted",     nop,                    NULL},
	{"exitondone",   set_exitondone,         "on|off"},
	{"random",       nop,                    NULL},
	{"rejected",     nop,                    NULL},
	{"random",       nop,                    NULL},
	{"computer",     nop,                    NULL},
	{"name",         nop,                    NULL},
	{"search",       cmd_search,             NULL},
	{"search_sync",  cmd_search_sync,        NULL},
	{"analyze",      cmd_analyze,            NULL},
	{"undo",         cmd_undo,               NULL},
	{"redo",         cmd_redo,               NULL},
	{"verbose",      set_verbosity,          "on|off"},
	{"setmovenot",   cmd_setmovenot,         "coor|san"},
	{"getmovenot",   cmd_getmovenot,         NULL},
	{"ping",         cmd_ping,               NULL},
	{"eval",         cmd_eval,               NULL},
	{"poskey",       cmd_poskey,             NULL},
	{"polyglot_key", cmd_polyglotkey,        NULL},
	{"hash_value",   cmd_hash_value,         NULL},
	{"hash_value_min",
		cmd_hash_value_exact_min,        NULL},
	{"hash_value_max",
		cmd_hash_value_exact_max,        NULL},
	{"position",     cmd_position,           NULL}
/* END CSTYLED */
};

static void
init_settings(void)
{
	is_xboard = false;
	exit_on_done = false;
	computer_side = black;
	qsort(cmd_list, ARRAY_LENGTH(cmd_list), sizeof *cmd_list,
	    (int (*)(const void*, const void*))strcmp);
	game = game_create();
	reset_engine(current_position());
	set_thinking_done_cb(computer_move);
	set_show_thinking(print_current_result);
	set_computer_clock(30000);
	set_opponent_clock(30000);
	set_moves_left_in_time(40);
	set_time_inc(0);
	game_started = false;
}

static void
dispatch_command(const char *cmd)
{
	const struct cmd_entry *e;
	char cmdlower[strlen(cmd) + 1];

	(void) str_to_lower(strcpy(cmdlower, cmd));
	e = bsearch(cmdlower, cmd_list,
	    ARRAY_LENGTH(cmd_list), sizeof *cmd_list,
	    (int (*)(const void*, const void*))strcmp);
	if (e == NULL) {
		(void) fprintf(stderr, "Error: (uknown command): %s\n", cmd);
		return;
	}
	if (setjmp(command_error) == 0) {
		e->cmd_func();
	}
	else switch (error_condition) {
		case CMD_PARAM_ERROR:
			if (e->paramstr != NULL)
				fprintf(stderr,
				    "Usage: %s %s\n", e->text, e->paramstr);
			break;
		case CMD_GENERAL_ERROR:
			(void) fprintf(stderr, "Unable to do that\n");
			break;
		default:
			INTERNAL_ERROR();
	}
}