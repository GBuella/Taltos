/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
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
#include <threads.h>

#include "book.h"
#include "perft.h"
#include "game.h"
#include "engine.h"
#include "taltos.h"
#include "hash.h"
#include "eval.h"
#include "str_util.h"
#include "trace.h"
#include "move_order.h"
#include "move_desc.h"

struct cmd_entry {
	const char text[16];
	void (*cmd_func)(void);
	const char *paramstr;
};

static struct taltos_conf *conf;
static struct book *book;
static jmp_buf command_error;
static int error_condition;
static char *line_lasts;
static uintmax_t callback_key;
static mtx_t stdout_mutex;
static mtx_t game_mutex;

static void
setup_mutexes(void)
{
	if (mtx_init(&stdout_mutex, mtx_plain | mtx_recursive) != thrd_success)
		abort();

	if (mtx_init(&game_mutex, mtx_plain | mtx_recursive) != thrd_success)
		abort();
}

static void
reset_mutexes(void)
{
	mtx_destroy(&stdout_mutex);
	mtx_destroy(&game_mutex);
	setup_mutexes();
}

#define CMD_PARAM_ERROR 1
#define CMD_GENERAL_ERROR 2

#define INTERNAL_ERROR() { \
	(void) fprintf(stderr, "Internal error in %s\n", __func__); \
	exit(EXIT_FAILURE); }

static void
param_error(void)
{
	error_condition = CMD_PARAM_ERROR;
	reset_mutexes();
	longjmp(command_error, 1);
}

static void
general_error(void)
{
	error_condition = CMD_GENERAL_ERROR;
	reset_mutexes();
	longjmp(command_error, 1);
}

static const char *features[] = {
	"ping=1",
	"setboard=1",
	"sigint=1",
	"reuse=1",
	"memory=1",
	NULL
};

static bool is_force_mode;
static enum player computer_side;
static bool is_xboard;
static bool is_uci;
static bool can_ponder;
static const char *whose_turn[] = {"whites", "blacks"};
static bool game_started;
static bool exit_on_done;
static bool verbose;
static char line[0x10000];

static struct game *game = NULL;

static const struct position*
current_position(void)
{
	return game_current_position(game);
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
is_checkmate(void)
{
	return game_is_checkmate(game);
}

static bool
is_stalemate(void)
{
	return game_is_stalemate(game);
}

static bool
is_draw_by_insufficient_material(void)
{
	return game_is_draw_by_insufficient_material(game);
}

static bool
is_draw_by_repetition(void)
{
	return game_is_draw_by_repetition(game);
}

static bool
is_draw_by_50_move_rule(void)
{
	return game_is_draw_by_50_move_rule(game);
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

static char*
printm(const struct position *pos, move m, char *str, enum player pl)
{
	return print_move(pos, m, str, conf->move_not, pl);
}

static void
add_move(move m)
{
	mtx_lock(&game_mutex);


	char move_str[MOVE_STR_BUFFER_LENGTH];

	(void) print_move(current_position(), m, move_str, mn_san, turn());
	if (game_append(game, m) == 0) {
		engine_process_move(m);
		debug_engine_set_player_to_move(turn());
		tracef("repro: %s", move_str);

		if (is_end()) {
			game_started = false;

			if (is_checkmate() && turn() == white)
				puts("0-1 {Black mates}");
			else if (is_checkmate() && turn() == black)
				puts("1-0 {White mates}");
			else if (is_stalemate())
				puts("1/2-1/2 {Stalemate}");
			else if (is_draw_by_insufficient_material())
				puts("1/2-1/2 {No mating material}");
			else if (is_draw_by_repetition())
				puts("1/2-1/2 {Draw by repetition}");
			else if (is_draw_by_50_move_rule())
				puts("1/2-1/2 {Draw by 50 move rule}");
			else
				abort();
		}
	}

	mtx_unlock(&game_mutex);
}


static void dispatch_command(char *cmd);

static void
set_xboard(void)
{
	is_xboard = true;
	puts("");
#if !defined(WIN32) && !defined(WIN64)
	(void) signal(SIGINT, SIG_IGN);
#endif
}

static void
set_uci(void)
{
	is_uci = true;
	printf("id name %s%s", conf->display_name, conf->display_name_postfix);
	printf("id author %s", author_name);
	printf("option name Hash type spin default %u min %u max %u\n",
	    conf->hash_table_size_mb, ht_min_size_mb(), ht_max_size_mb());
	puts("uciok");
}

static char*
get_str_arg_opt(void)
{
	return xstrtok_r(NULL, " \t\n\r", &line_lasts);
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

static long long
get_num_arg(const char *str)
{
	long long n;
	char *endptr;

	n = strtoll(str, &endptr, 0);

	if (n == LLONG_MAX || n == LLONG_MIN || *endptr != '\0') {
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
	unsigned move_counter;
	bool is_black;

	if (is_xboard || is_uci)
		mn = mn_coordinate;
	else
		mn = conf->move_not;

	mtx_lock(&game_mutex);

	(void) print_move(current_position(), m, str, mn, turn());
	move_counter = game_full_move_count(game);
	is_black = (turn() == black);

	mtx_unlock(&game_mutex);

	mtx_lock(&stdout_mutex);

	tracef("%s %s", __func__, str);

	if (is_xboard) {
		printf("move %s\n", str);
	}
	else if (is_uci) {
		printf("bestmove %s\n", str);
	}
	else {
		printf("%u. ", move_counter);
		if (is_black)
			printf("... ");
		puts(str);
	}

	mtx_unlock(&stdout_mutex);
}

static int
computer_move(uintmax_t key)
{
	if (key != callback_key)
		return -1;

	move m;

	mtx_lock(&game_mutex);

	if (engine_get_best_move(&m) != 0) {
		puts("-");
	}
	else {
		print_computer_move(m);
		if (exit_on_done)
			exit(EXIT_SUCCESS);
		add_move(m);
		engine_move_count_inc();
	}

	mtx_unlock(&game_mutex);

	return 0;
}

static void
decide_move(void)
{
	mtx_lock(&game_mutex);

	if (game_started && !is_end() && !is_force_mode) {
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
				set_thinking_done_cb(computer_move,
				    ++callback_key);
				start_thinking();
			}
		}
	}
	else {
		game_started = false;
	}

	mtx_unlock(&game_mutex);
}

static void
operator_move(move m)
{
	stop_thinking();

	mtx_lock(&game_mutex);

	if (!is_force_mode)
		game_started = true;
	add_move(m);
	decide_move();

	mtx_unlock(&game_mutex);
}

static void
print_move_path(const struct game *original_game, const move *m)
{
	char str[MOVE_STR_BUFFER_LENGTH];
	bool first = true;
	struct game *g = game_copy(original_game);

	if (g == NULL)
		INTERNAL_ERROR();
	for (; *m != 0; ++m) {
		if (!is_uci) {
			if (game_turn(g) == white || first)
				printf("%u. ", game_full_move_count(g));
			if (first && game_turn(g) == black)
				printf("... ");
		}
		first = false;
		(void) printm(game_current_position(g), *m, str, game_turn(g));
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
		printf("  D\tQD\ttime\tvalue\tfmc\thuse\tnodes\tqnodes\tPV\n");
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
	mtx_lock(&game_mutex);
	mtx_lock(&stdout_mutex);

	if (is_xboard) {
		printf("%u ", res.depth);
		if (res.sresult.value < - mate_value)
			printf("%d ",
			    -100000 - (res.sresult.value + max_value) / 2);
		else if (res.sresult.value > mate_value)
			printf("%d ",
			    100000 + (max_value - res.sresult.value) / 2);
		else
			printf("%d ", res.sresult.value);
		printf("%ju ", res.time_spent);
		printf("%ju ", res.sresult.node_count);
	}
	else if (is_uci) {
		printf("info depth %u ", res.depth);
		printf("seldepth %u ", res.sresult.selective_depth);
		if (res.sresult.value < - mate_value)
			printf("score mate -%d ",
			    (res.sresult.value + max_value) / 2);
		else if (res.sresult.value > mate_value)
			printf("score mate %d ",
			    (max_value - res.sresult.value) / 2);
		else
			printf("score cp %d ", res.sresult.value);
		printf("nodes %ju ", res.sresult.node_count);
	}
	else {
		if (res.first)
			print_result_header();
		putchar(' ');
		print_depth(res);
		putchar('\t');
		printf("%ju.%.2ju", res.time_spent / 100, res.time_spent % 100);
		putchar('\t');
		if (res.sresult.value < - mate_value)
			printf("-#%d", (res.sresult.value + max_value) / 2);
		else if (res.sresult.value > mate_value)
			printf("#%d", (max_value - res.sresult.value) / 2);
		else
			print_centipawns(res.sresult.value);
		putchar('\t');
		if (verbose)
			print_verbose_search_info(res);
		(void) print_nice_count(res.sresult.node_count);
		printf("N\t");
	}

	if (is_uci)
		printf("pv ");
	print_move_path(game, res.pv);
	putchar('\n');

	mtx_unlock(&stdout_mutex);
	mtx_unlock(&game_mutex);
}

static int
try_read_move(const char *cmd)
{
	move move;

	if (is_end())
		return 1;

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
	trace(__func__);

	char *cmd;

	assert(arg_conf != NULL && arg_book != NULL);
	conf = arg_conf;
	book = arg_book;
	init_settings();

	setup_mutexes();

	while (true) {
		if (fgets(line, sizeof line, stdin) == NULL) {
#ifdef EINTR
			if (errno == EINTR)
				continue;
#endif
			if (exit_on_done)
				wait_thinking();
			exit(EXIT_SUCCESS);
		}

		if (line[0] == '\0')
			continue;

		if (strlen(line) == sizeof(line) - 1)
			abort();

		line[strcspn(line, "\n\r")] = '\0';

		tracef("%s input: \"%s\"", __func__, line);

		bool is_it_move;

		if ((cmd = xstrtok_r(line, " \t\n", &line_lasts)) == NULL)
			continue;

		mtx_lock(&game_mutex);

		is_it_move = (try_read_move(cmd) == 0);

		mtx_unlock(&game_mutex);

		if (!is_it_move)
			dispatch_command(cmd);
	}
}

static void
cmd_quit(void)
{
	callback_key++;
	stop_thinking();
	exit(EXIT_SUCCESS);
}

static int
get_int(int min, int max)
{
	long long n;

	n = get_num_arg(get_str_arg());
	if (n < (long long)min) {
		(void) fprintf(stderr, "Number too low: %lld\n", n);
		param_error();
	}
	if (n > (long long)max) {
		(void) fprintf(stderr, "Number too high: %lld\n", n);
		param_error();
	}
	return (int)n;
}

static unsigned
get_uint(unsigned min, unsigned max)
{
	long long n;

	n = get_num_arg(get_str_arg());
	if (n < (long long)min) {
		(void) fprintf(stderr, "Number too low: %lld\n", n);
		param_error();
	}
	if (n > (long long)max) {
		(void) fprintf(stderr, "Number too high: %lld\n", n);
		param_error();
	}
	return (unsigned)n;
}

static void
cmd_perft(void)
{
	printf("%" PRIuMAX "\n", perft(current_position(), get_uint(1, 1024)));
}

static void
cmd_perfto(void)
{
	printf("%" PRIuMAX "\n", perft_ordered(current_position(),
	    get_uint(1, 1024)));
}

static void
cmd_qperft(void)
{
	printf("%" PRIuMAX "\n", qperft(current_position(), get_uint(1, 1024)));
}

static void
cmd_perfts(void)
{
	unsigned depth;

	depth = get_uint(1, 1024);
	for (unsigned i = 1; i <= depth; ++i) {
		uintmax_t n = perft(current_position(), i);
		printf("%s%u : %" PRIuMAX "\n", (i < 10) ? " " : "", i, n);
	}
}

static void
run_cmd_divide(bool ordered)
{
	struct divide_info *dinfo;
	const char *line;

	mtx_lock(&game_mutex);

	dinfo = divide_init(current_position(),
	    get_uint(0, 1024),
	    turn(),
	    ordered);

	mtx_unlock(&game_mutex);

	mtx_lock(&stdout_mutex);

	while ((line = divide(dinfo, conf->move_not)) != NULL)
		puts(line);
	mtx_unlock(&stdout_mutex);

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

	g = game_create_fen(xstrtok_r(NULL, "\n\r", &line_lasts));
	if (g == NULL) {
		(void) fprintf(stderr, "Unable to parse FEN\n");
		return;
	}

	mtx_lock(&game_mutex);

	game_destroy(game);
	game = g;
	reset_engine(current_position());
	debug_engine_set_player_to_move(turn());

	mtx_unlock(&game_mutex);
}

static void
cmd_printboard(void)
{
	char str[BOARD_BUFFER_LENGTH];

	mtx_lock(&game_mutex);

	(void) board_print(str, current_position(), turn(), conf->use_unicode);

	mtx_unlock(&game_mutex);

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

	if ((str = xstrtok_r(NULL, "\n\r", &line_lasts)) != NULL)
		puts(str);
}

static void
cmd_new(void)
{
	callback_key++;
	stop_thinking();

	mtx_lock(&game_mutex);

	game_destroy(game);
	if ((game = game_create()) == NULL)
		INTERNAL_ERROR();
	reset_engine(current_position());
	debug_engine_set_player_to_move(turn());
	computer_side = black;
	set_thinking_done_cb(computer_move, ++callback_key);
	unset_search_depth_limit();
	if (!(is_xboard || is_uci))
		puts("New game - computer black");
	game_started = false;
	is_force_mode = false;

	mtx_unlock(&game_mutex);
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

	mtx_lock(&game_mutex);

	if (engine_get_best_move(&m) == 0) {
		printm(current_position(), m, str, turn());
		printf("Hint: %s\n", str);
	}

	mtx_unlock(&game_mutex);
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

/*
 * Xboard - level command
 * Arguments are three numbers, or four numbers with a colon between
 * the two in the middle:
 *
 *  level 40 5 0
 *
 * or
 *
 *  level 20 1:40 2
 */
static void
cmd_level(void)
{
	unsigned mps, base, inc;
	char *base_str;
	char *base_token;
	char *lasts;
	long long base_minutes;
	long long base_seconds;

	mps = get_uint(0, 1024);
	base_str = get_str_arg();
	inc = get_uint(0, 1024);

	base_token = xstrtok_r(base_str, ":", &lasts);
	base_minutes = get_num_arg(base_token);
	if (base_minutes < 0 || base_minutes > 8192)
		param_error();

	if ((base_token = xstrtok_r(NULL, ":", &lasts)) != NULL)
		base_seconds = get_num_arg(base_token);
	else
		base_seconds = 0;

	if (base_minutes < 0 || base_minutes > 59)
		param_error();

	if (xstrtok_r(NULL, ":", &lasts) != NULL)
		param_error();

	base = ((unsigned)base_minutes) * 60 + (unsigned)base_seconds;
	set_moves_left_in_time(mps);
	set_computer_clock(base * 100);
	set_time_inc(inc * 100);
}

static void
cmd_protover(void)
{
	mtx_lock(&stdout_mutex);

	printf("feature");

	for (const char **f = features; *f != NULL; ++f)
		printf(" %s", *f);

	printf(" myname=\"%s%s\"\n",
	       conf->display_name, conf->display_name_postfix);
	puts("feature done=1");

	mtx_unlock(&stdout_mutex);
}

static void
cmd_force(void)
{
	callback_key++;
	is_force_mode = true;
	game_started = false;
	stop_thinking();
}

static void
cmd_playother(void)
{
	callback_key++;
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
cmd_sti(void)
{
	set_time_infinite();
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

static int
search_cb(uintmax_t key)
{
	if (key != callback_key)
		return -1;

	move m;

	mtx_lock(&game_mutex);

	if (engine_get_best_move(&m) == 0) {
		print_computer_move(m);
		set_thinking_done_cb(computer_move, ++callback_key);
	}
	if (exit_on_done)
		exit(EXIT_SUCCESS);

	mtx_unlock(&game_mutex);

	return 0;
}

static void
cmd_search(void)
{
	if (game_started)
		return;
	set_thinking_done_cb(search_cb, ++callback_key);
	start_thinking_single_thread();
}

static void
cmd_search_sync(void)
{
	if (game_started)
		return;
	set_thinking_done_cb(search_cb, ++callback_key);
	start_thinking_single_thread();
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
		debug_engine_set_player_to_move(turn());
	}
}

static void
cmd_redo(void)
{
	mtx_lock(&game_mutex);

	if (is_force_mode) {
		if (forward() != 0)
			general_error();
		reset_engine(current_position());
		debug_engine_set_player_to_move(turn());
	}

	mtx_unlock(&game_mutex);
}

static void
cmd_setmovenot(void)
{
	char *str = get_str_arg_lower();

	if (strcmp(str, "coor") == 0)
		conf->move_not = mn_coordinate;
	else if (strcmp(str, "san") == 0)
		conf->move_not = mn_san;
	else if (conf->use_unicode && strcmp(str, "fan") == 0)
		conf->move_not = mn_fan;
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
		case mn_fan:
			puts("Using Figurine algebraic notation");
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

	mtx_lock(&stdout_mutex);

	if ((str = get_str_arg_opt()) == NULL)
		puts("pong");
	else
		printf("pong %s\n", str);

	mtx_unlock(&stdout_mutex);
}

static void
cmd_isready(void)
{
	mtx_lock(&stdout_mutex);

	puts("readyok");

	mtx_unlock(&stdout_mutex);
}

static void
cmd_eval(void)
{
	struct eval_factors ef = compute_eval_factors(current_position());
	printf(" material:            ");
	print_centipawns(ef.material);
	printf("\n basic_mobility:      ");
	print_centipawns(ef.basic_mobility);
	printf("\n center_control:      ");
	print_centipawns(ef.center_control);
	printf("\n threats:             ");
	print_centipawns(ef.threats);
	printf("\n pawn_structure:      ");
	print_centipawns(ef.pawn_structure);
	printf("\n passed_pawns:        ");
	print_centipawns(ef.passed_pawns);
	printf("\n king_safety:         ");
	print_centipawns(ef.king_safety);
	printf("\n rook_placement:      ");
	print_centipawns(ef.rook_placement);
	printf("\n knight_placement:    ");
	print_centipawns(ef.knight_placement);
	printf("\n bishop_placement:    ");
	print_centipawns(ef.bishop_placement);
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
cmd_hash_size(void)
{
	mtx_lock(&stdout_mutex);

	/* BEGIN CSTYLED */ /* cstyle does not know about this syntax yet */
	print_nice_number(engine_ht_size(),
	    (const char *[]) {"b", "kb", "mb", "gb", NULL},
	    (const uintmax_t[]) {1, 1024, 1024 * 1024, 1024 * 1024 * 1024, 0});
	/* END CSTYLED */

	mtx_unlock(&stdout_mutex);
}

static void
cmd_hash_entry(void)
{
	ht_entry entry;
	char *fen = xstrtok_r(NULL, "\n\r", &line_lasts);

	if (fen != NULL) {
		struct game *g = game_create_fen(fen);
		if (g == NULL) {
			(void) fprintf(stderr, "Unable to parse FEN\n");
			return;
		}
		entry = engine_get_entry(game_current_position(g));
		game_destroy(g);
	}
	else {
		entry = engine_current_entry();
	}

	if (!ht_is_set(entry)) {
		puts("hash_value: none");
		return;
	}

	mtx_lock(&stdout_mutex);

	printf("hash_depth: %d\n", ht_depth(entry));

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

	mtx_unlock(&stdout_mutex);
}

static void
cmd_hash_value_exact_min(void)
{
	int minimum = get_int(-max_value, max_value);

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
	int maximum = get_int(-max_value, max_value);

	ht_entry entry = engine_current_entry();
	if (ht_is_set(entry) && ht_value_type(entry) == vt_exact
	    && ht_value(entry) <= maximum)
		puts("ok");
	else
		puts("no");
}

static void
display_position_info(void)
{
	mtx_lock(&stdout_mutex);

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

	mtx_unlock(&stdout_mutex);
}

static int
process_uci_move_list(struct game *g)
{
	const char *next;

	while ((next = xstrtok_r(NULL, " \t\n\r", &line_lasts)) != NULL) {
		move m;

		if (read_move(game_current_position(g),
		    next, &m, game_turn(g)) != 0)
			return -1;

		if (game_append(g, m) != 0)
			return -1;
	}

	return 0;
}

static int
process_uci_starting_position(const char *next_token, struct game *g)
{
	if (strcmp(next_token, "fen") == 0) {
		char *fen_start;
		char *fen_last;

		fen_start = xstrtok_r(NULL, " \t\n\r", &line_lasts); // board
		xstrtok_r(NULL, " \t\n\r", &line_lasts); // turn
		xstrtok_r(NULL, " \t\n\r", &line_lasts); // castle rights
		fen_last = xstrtok_r(NULL, " \t\n\r", &line_lasts); // ep
		if (fen_last == NULL) {
			fprintf(stderr, "Invalid FEN\n");
			return -1;
		}
		next_token = xstrtok_r(NULL, " \t\n\r", &line_lasts);
		if (strcmp(next_token, "moves") != 0) {
			fen_last = xstrtok_r(NULL, " \t\n\r", &line_lasts);
			next_token = xstrtok_r(NULL, " \t\n\r", &line_lasts);
			if (fen_last == NULL) {
				fprintf(stderr, "Invalid FEN\n");
				return -1;
			}
		}
		for (char *c = fen_start; c != fen_last; ++c) {
			if (*c == '\0')
				*c = ' ';
		}
		if (game_reset_fen(g, fen_start) != 0) {
			fprintf(stderr, "Invalid FEN\n");
			return -1;
		}
	}
	else if (strcmp(next_token, "startpos") != 0) {
		return -1;
	}
	else {
		next_token = xstrtok_r(NULL, " \t\n\r", &line_lasts);
	}

	if (next_token != NULL) {
		if (strcmp(next_token, "moves") == 0) {
			if (process_uci_move_list(g) != 0)
				return -1;
		}
	}

	return 0;
}

static void
replay_new_moves(struct game *g)
{
	size_t delta_count;

	game_truncate(game);
	delta_count = game_length(g) - game_length(game);

	while (delta_count-- != 0)
		game_history_revert(g);

	while (game_move_to_next(g) != 0) {
		add_move(game_move_to_next(g));
		game_history_forward(g);
	}
}

static void
cmd_position(void)
{
	const char *arg;

	arg = xstrtok_r(NULL, " \t\n\r", &line_lasts);

	if (arg == NULL) {
		display_position_info();
	}
	else {
		cmd_force();

		mtx_lock(&stdout_mutex);
		mtx_lock(&game_mutex);

		struct game *g = game_create();

		if (process_uci_starting_position(arg, g) == 0) {
			if (game_continues(g, game)) {
				replay_new_moves(g);
			}
			else {
				struct game *t = game;
				game = g;
				g = t;
				reset_engine(current_position());
				debug_engine_set_player_to_move(turn());
			}
		}

		game_destroy(g);

		mtx_unlock(&game_mutex);
		mtx_unlock(&stdout_mutex);
	}
}

static void
cmd_memory(void)
{
	unsigned value = get_uint(ht_min_size_mb(), ht_max_size_mb());

	tracef("repro: memory %u", value);

	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value = (value + 1) >> 1;

	mtx_lock(conf->mutex);
	conf->hash_table_size_mb = value;
	mtx_unlock(conf->mutex);
	engine_conf_change();
}

static void
cmd_setoption(void)
{
	const char *token;
	const char *name;

	token = xstrtok_r(NULL, " \t\n\r", &line_lasts);
	if (token == NULL || strcmp(token, "name") != 0)
		return;

	name = xstrtok_r(NULL, " \t\n\r", &line_lasts);
	if (name == NULL)
		return;

	token = xstrtok_r(NULL, " \t\n\r", &line_lasts);
	if (token == NULL || strcmp(token, "value") != 0)
		return;

	if (strcmp(name, "Hash") == 0) {
		cmd_memory();
	}
	else {
		return;
	}
}

static void
cmd_go(void)
{
	const char *token;

	if (game_started && is_comp_turn())
		return;

	if (!is_comp_turn())
		computer_side = opponent_of(computer_side);

	while ((token = xstrtok_r(NULL, " \t\n\r", &line_lasts)) != NULL) {
		if (strcmp(token, "infinite") == 0)
			cmd_sti();
		else if (strcmp(token, "wtime") == 0 && computer_side == white)
			set_computer_clock(get_uint(0, UINT_MAX) / 10);
		else if (strcmp(token, "winc") == 0 && computer_side == white)
			set_time_inc(get_uint(0, UINT_MAX) / 10);
		else if (strcmp(token, "btime") == 0 && computer_side == black)
			set_computer_clock(get_uint(0, UINT_MAX) / 10);
		else if (strcmp(token, "binc") == 0 && computer_side == black)
			set_time_inc(get_uint(0, UINT_MAX) / 10);
		else if (strcmp(token, "movestogo") == 0)
			set_moves_left_in_time(get_uint(0, 1024));
		else if (strcmp(token, "nodes") == 0)
			set_exact_node_count(get_uint(1, UINT_MAX));
	}

	is_force_mode = false;
	game_started = true;
	decide_move();
}

static void
cmd_ucinewgame(void)
{
	stop_thinking();
}

static void
cmd_stop(void)
{
	stop_thinking();
}

static void
print_move_order(const struct position *pos, enum player player)
{
	struct move_order move_order;
	move_order_setup(&move_order, pos, false, 0);

	if (move_order.count == 0) {
		puts("No legal moves");
		return;
	}

	ht_entry entry = engine_get_entry(pos);
	if (ht_is_set(entry) && ht_has_move(entry))
		move_order_add_hint(&move_order, ht_move(entry), 0);

	do {
		move_order_pick_next(&move_order);

		char buf[MOVE_STR_BUFFER_LENGTH];
		move m = mo_current_move(&move_order);
		int value = mo_current_move_value(&move_order);

		(void) printm(pos, m, buf, player);

		printf("#%u %s %d\n", move_order.picked_count, buf, value);
	} while (!move_order_done(&move_order));
}

static int
cmp_move(const void *a, const void *b)
{
	move ma = *((move*)a);
	move mb = *((move*)b);

	if (ma > mb)
		return 1;
	else if (ma < mb)
		return -1;
	else
		return 0;
}

static void
print_move_desc(const char *move_str, const struct move_desc *desc)
{
	printf("%s\tSEE loss on source square: %d\n",
	       move_str, desc->src_sq.SEE_loss);
	printf("\tattacks from source square: 0x%016" PRIx64 "\n",
	       desc->src_sq.attacks);
	printf("\tattacks on source square: 0x%016" PRIx64 "\n",
	       desc->src_sq.attackers);
	printf("\tSEE loss on destination square: %d\n", desc->dst_sq.SEE_loss);
	printf("\tattacks from destination square: 0x%016" PRIx64 "\n",
	       desc->dst_sq.attacks);
	printf("\tattacks on destination square: 0x%016" PRIx64 "\n",
	       desc->dst_sq.attackers);
	printf("\tSEE value: %d\n", desc->SEE_value);
	printf("\tdiscovered_attacks: 0x%016" PRIx64 "\n",
	       desc->discovered_attacks);
	printf("\tvalue: %d\n", desc->value);
	if (desc->direct_check)
		printf("\tdirect check\n");
	if (desc->discovered_check)
		printf("\tdiscovered check\n");
}

static void
print_move_descs(const struct position *pos, enum player player)
{
	move moves[MOVE_ARRAY_LENGTH];

	unsigned count = gen_moves(pos, moves);
	if (count == 0)
		return;

	struct move_desc desc;
	move_desc_setup(&desc);
	qsort(moves, count, sizeof(moves[0]), cmp_move);

	for (unsigned i = 0; i < count; ++i) {
		char str[MOVE_STR_BUFFER_LENGTH];
		printm(pos, moves[i], str, player);
		describe_move(&desc, pos, moves[i]);
		print_move_desc(str, &desc);
		putchar('\n');
	}
}

static void
cmd_with_optional_fen_arg(void (*cmd)(const struct position*, enum player))
{
	char *fen = xstrtok_r(NULL, "\n\r", &line_lasts);

	if (fen != NULL) {
		struct game *g = game_create_fen(fen);
		if (g == NULL) {
			(void) fprintf(stderr, "Unable to parse FEN\n");
			return;
		}
		cmd(game_current_position(g), game_turn(g));
		game_destroy(g);
	}
	else {
		cmd(current_position(), turn());
	}
}

static void
cmd_md(void)
{
	cmd_with_optional_fen_arg(print_move_descs);
}

static void
cmd_mo(void)
{
	cmd_with_optional_fen_arg(print_move_order);
}

static void
cmd_nodes(void)
{
	set_exact_node_count(get_uint(1, UINT_MAX));
}

static void
cmd_booksize(void)
{
	printf("%zu\n", book_get_size(book));
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
	{"sti",          cmd_sti,                NULL},
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
	{"hash_size",    cmd_hash_size,          NULL},
	{"hash_entry",   cmd_hash_entry,         NULL},
	{"hash_value_min",
		cmd_hash_value_exact_min,        NULL},
	{"hash_value_max",
		cmd_hash_value_exact_max,        NULL},
	{"position",     cmd_position,           NULL},
	{"memory",       cmd_memory,             NULL},
	{"uci",          set_uci,                NULL},
	{"isready",      cmd_isready,            NULL},
	{"setoption",    cmd_setoption,          NULL},
	{"ucinewgame",   cmd_ucinewgame,         NULL},
	{"stop",         cmd_stop,               NULL},
	{"mo",           cmd_mo,                 NULL},
	{"nodes",        cmd_nodes,              NULL},
	{"booksize",     cmd_booksize,           NULL},
	{"md",           cmd_md,                 NULL}
/* END CSTYLED */
};

static void
init_settings(void)
{
	is_xboard = false;
	is_uci = false;
	exit_on_done = false;
	computer_side = black;
	qsort(cmd_list, ARRAY_LENGTH(cmd_list), sizeof *cmd_list,
	    (int (*)(const void*, const void*))strcmp);
	game = game_create();
	reset_engine(current_position());
	debug_engine_set_player_to_move(turn());
	set_thinking_done_cb(computer_move, callback_key);
	set_show_thinking(print_current_result);
	set_computer_clock(30000);
	set_opponent_clock(30000);
	set_moves_left_in_time(40);
	set_time_inc(0);
	game_started = false;
}

static void
dispatch_command(char *cmd)
{
	const struct cmd_entry *e;

	(void) str_to_lower(cmd);
	e = bsearch(cmd, cmd_list,
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
