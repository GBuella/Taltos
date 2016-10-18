
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

static void
add_move(move m)
{
	mtx_lock(&game_mutex);

	if (game_append(game, m) == 0) {
		engine_process_move(m);
		debug_engine_set_player_to_move(turn());

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

	if (is_xboard)
		mn = mn_coordinate;
	else
		mn = conf->move_not;

	mtx_lock(&game_mutex);

	(void) print_move(current_position(), m, str, mn, turn());
	move_counter = game_full_move_count(game);
	is_black = (turn() == black);

	mtx_unlock(&game_mutex);

	mtx_lock(&stdout_mutex);

	if (is_xboard) {
		printf("move %s\n", str);
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
	mtx_lock(&game_mutex);
	mtx_lock(&stdout_mutex);

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

	mtx_unlock(&stdout_mutex);
	mtx_unlock(&game_mutex);
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

	setup_mutexes();

	while (true) {
		if (fgets(line, sizeof line, stdin) == NULL) {
			if (exit_on_done)
				wait_thinking();
			exit(EXIT_SUCCESS);
		}

		if (line[0] == '\0')
			continue;

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

	(void) board_print(str, current_position(), turn());

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
	if (!is_xboard)
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
		print_move(current_position(), m, str, conf->move_not, turn());
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

	printf(" myname=\"%s\"\n", conf->display_name);
	puts("feature done=1");

	mtx_unlock(&stdout_mutex);
}

static void
cmd_force(void)
{
	callback_key++;
	is_force_mode = true;
	stop_thinking();
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
	printf("\n basic_mobility:      ");
	print_centipawns(ef.basic_mobility);
	printf("\n center_control:      ");
	print_centipawns(ef.center_control);
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
	printf("\n queen_placement:     ");
	print_centipawns(ef.queen_placement);
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
	{"hash_entry",   cmd_hash_entry,         NULL},
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
