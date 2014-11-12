
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
#include "trace.h"
#include "eval.h"
#include "timers.h"
#include "str_util.h"

struct cmd_entry {
    const char text[16];
    void (*cmd_func)(void);
    const char *paramstr;
};

static struct taltos_conf *horse;
static struct book *book;
static jmp_buf command_error;

#define CMD_PARAM_ERROR 1
#define CMD_GENERAL_ERROR 2

#define INTERNAL_ERROR() { \
    (void)fprintf(stderr, "Internal error in %s\n", __func__); \
    exit(EXIT_FAILURE); }

static void param_error(void)
{
    longjmp(command_error, CMD_PARAM_ERROR);
}

static void general_error(void)
{
    longjmp(command_error, CMD_GENERAL_ERROR);
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
static player computer_side;
static bool is_xboard;
static bool can_ponder;
static const char *whose_turn[] = {"whites", "blacks"};
static bool game_started;
static bool exit_on_done;
static bool verbose = false;

static struct game *game = NULL;

static void print(const char format[], ...)
{
    va_list args;
    va_start(args, format);
    if (vprintf(format, args) < 0) INTERNAL_ERROR();
    va_end(args);
}

static void add_move(move m)
{
    if (game_append(game, m) != 0) INTERNAL_ERROR();
}

static int revert(void)
{
    return game_history_revert(game);
}

static int forward(void)
{
    return game_history_forward(game);
}

static enum player turn(void)
{
    return game_turn(game);
}

const struct position *current_position(void)
{
    return game_current_position(game);
}

static bool is_comp_turn(void)
{
    return turn() == computer_side;
}

static bool is_opp_turn(void)
{
    return !is_comp_turn();
}

static bool is_end(void)
{
    return game_is_ended(game);
}

static bool has_single_response(void)
{
    return game_has_single_response(game);
}

static move get_single_response(void)
{
    return game_get_single_response(game);
}

static void dispatch_command(const char *cmd);

static void set_xboard(void)
{
    is_xboard = true;
    print("\n");
#   if !defined(WIN32) && !defined(WIN64)
    (void)signal(SIGINT, SIG_IGN);
#   endif
}

static char *get_str_arg_opt(void)
{
    return strtok(NULL, " \t\n\r");
}

static char *get_str_arg(void)
{
    char *str;

    if ((str = get_str_arg_opt()) == NULL) {
        param_error();
    }
    return str;
}

static long get_long_arg(void)
{
    long n;
    char *str;
    char *endptr;

    str = get_str_arg();
    n = strtol(str, &endptr, 0);
    if (n == LONG_MAX || n == LONG_MIN || *endptr != '\0') {
        (void)fprintf(stderr, "Invalid numeric argument\n");
        param_error();
    }
    return n;
}

static char *str_to_lower(char *str)
{
    for (char *c = str; *c != '\0'; ++c) {
        *c = (char)tolower(*c);
    }
    return str;
}

static char *get_str_arg_lower_opt(void)
{
    char *str;

    if ((str = get_str_arg_opt()) != NULL) {
        (void)str_to_lower(str);
    }
    return str;
}

static char *get_str_arg_lower(void)
{
    return str_to_lower(get_str_arg());
}

static void print_computer_move(move m)
{
    char str[MOVE_STR_BUFFER_LENGTH];
    enum move_notation_type mn;

    if (is_xboard) {
        mn = mn_coordinate;
    }
    else {
        mn = horse->move_not;
    }
    (void)print_move(current_position(), m, str, mn, turn());
    if (is_xboard) {
        print("move %s\n", str);
    }
    else {
        print("%d. ", game_full_move_count(game));
        if (turn() == black) {
            print("... ");
        }
        print("%s\n", str);
    }
}

static void decide_move(void)
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
            if (m != NONE_MOVE) {
                print_computer_move(m);
                add_move(m);
                engine_move_count_inc();
            }
            else {
                set_engine_root_node(current_position());
                timers_reset();
                start_thinking();
            }
        }
    }
    else {
        game_started = false;
    }
}

static void operator_move(move m)
{
    stop_thinking();
    if (!is_force_mode) {
        game_started = true;
    }
    add_move(m);
    decide_move();
}

static void print_move_path(const struct game *original_game,
                            const move *m,
                            enum move_notation_type mn)
{
    char str[MOVE_STR_BUFFER_LENGTH];
    bool first = true;
    struct game *g = game_copy(original_game);

    if (g == NULL) INTERNAL_ERROR();
    for (; *m != 0; ++m) {
        if (game_turn(g) == white || first) {
            print("%u. ", game_full_move_count(g));
        }
        if (first && game_turn(g) == black) {
            print("... ");
        }
        first = false;
        (void)print_move(game_current_position(g), *m, str, mn, game_turn(g));
        print("%s ", str);
        if (game_append(g, *m) != 0) INTERNAL_ERROR();
    }
    game_destroy(g);
}

static void print_percent(int p)
{
    if (p == -1) {
        print("-");
    }
    else {
        print("%d.%d%%", p/10, p%10);
    }
}

static void print_result_header(void)
{
    if (verbose) {
        print("  D\tQD\ttime\tvalue\tfmc\thuse\tnodes\tPV\n");
    }
    else {
        print("  D\ttime\tvalue\tnodes\tPV\n");
    }
}

static void print_verbose_search_info(struct engine_result res)
{
    print_percent(get_fmc_percent());
    print("\t");
    print_percent(ht_usage(res.ht_main));
    print("\t");
}

static void print_depth(struct engine_result res)
{
    print("%u", res.depth);
    if (!is_xboard) {
        print("/%d", (res.selective_depth > 0) ? res.selective_depth : 0);
        if (verbose) {
            print("\t%d", (res.qdepth > 0) ? res.qdepth : 0);
        }
    }
}

static void print_current_result(struct engine_result res)
{
    if (is_xboard) {
        print_depth(res);
        print(" %.0f %.0f %" PRIuMAX " ",
                res.value*100, res.time_spent*100, res.node_count);
    }
    else {
        if (res.first) {
            print_result_header();
        }
        print(" ");
        print_depth(res);
        print("\t%.2f\t%.2f\t", res.time_spent, res.value);
        if (verbose) {
            print_verbose_search_info(res);
        }
        (void)print_nice_count(res.node_count);
        print("N\t");
    }
    print_move_path(game, res.pv, horse->move_not);
    print("\n");
}

static void computer_move(void)
{
    move m;

    timers_print(horse->use_unicode);
    if (engine_get_best_move(&m) != 0) {
        print("-\n");
        return;
    }
    print_computer_move(m);
    if (exit_on_done) {
        exit(EXIT_SUCCESS);
    }
    add_move(m);
    engine_move_count_inc();
}

static int try_read_move(const char *cmd)
{
    move move;

    switch (read_move(current_position(), cmd, &move, turn())) {
        case NONE_MOVE:
            return 1;
        case ILLEGAL_MOVE:
            print("Illegal move: %s\n", cmd);
            return 0;
        case 0:
            if (!is_force_mode && !is_opp_turn()) {
                print("It is not %s's turn\n",
                        whose_turn[opponent(computer_side)]);
                return 0;
            }
            trace("Operator move: %s\n", cmd);
            operator_move(move);
            return 0;
        default: assert(0);
    }
    return 0;
}

static void init_settings(void);

void loop_cli(struct taltos_conf *arg_horse, struct book *arg_book)
{
    char line[1024];
    char *cmd;

    assert(arg_horse != NULL && arg_book != NULL);
    trace("Command loop starting");
    horse = arg_horse;
    book = arg_book;
    init_settings();
    while (true) {
        if (fgets(line, sizeof line, stdin) == NULL) {
            if (exit_on_done) {
                wait_thinking();
            }
            exit(EXIT_SUCCESS);
        }
        if ((cmd = strtok(line, " \t\n")) == NULL) {
            continue;
        }
        if (try_read_move(cmd) != 0) {
            dispatch_command(cmd);
        }
    }
}

static void cmd_quit(void)
{
    stop_thinking();
    exit(EXIT_SUCCESS);
}

static unsigned get_uint(unsigned min, unsigned max)
{
    long n;

    n = get_long_arg();
    if (n < (long)min) {
        (void)fprintf(stderr, "Number too low: %ld\n", n);
        param_error();
    }
    if (n > (long)max) {
        (void)fprintf(stderr, "Number too high: %ld\n", n);
        param_error();
    }
    return (unsigned)n;
}

static void cmd_perft(void)
{
    print("%lu\n", perft(current_position(), get_uint(1, 1024)));
}

static void cmd_perfto(void)
{
    print("%lu\n", perft_ordered(current_position(), get_uint(1, 1024)));
}

static void cmd_perft_distinct(void)
{
    print("%ld\n", perft_distinct(current_position(), get_uint(1, 1024)));
}

static void cmd_perfts(void)
{
    unsigned depth;

    depth = get_uint(1, 1024);
    for (unsigned i = 1; i <= depth; ++i) {
        unsigned long n = perft(current_position(), i);
        print("%s%u : %ld\n", (i<10) ? " " : "", i, n);
    }
}

static void run_cmd_divide(bool ordered)
{
    struct divide_info *dinfo;
    const char *line;

    dinfo = divide_init(current_position(),
                        get_uint(0, 1024),
                        turn(),
                        ordered);
    while ((line = divide(dinfo, horse->move_not)) != NULL) {
        print("%s\n", line);
    }
    divide_destruct(dinfo);
}

static void cmd_divide(void)
{
    run_cmd_divide(false);
}

static void cmd_divideo(void)
{
    run_cmd_divide(true);
}

static void cmd_setboard(void)
{
    struct game *g;

    if (game_started) return;
    if ((g = game_create_fen(strtok(NULL, "\n\r"))) == NULL) {
        (void)fprintf(stderr, "Unable to parse FEN\n");
        return;
    }
    game_destroy(game);
    game = g;
    set_engine_root_node(current_position());
}

static void cmd_printboard(void)
{
    char str[BOARD_BUFFER_LENGTH];

    (void)board_print(str, current_position(), turn());
    (void)fputs(str, stdout);
}

static void cmd_printfen(void)
{
    char str[FEN_BUFFER_LENGTH];

    (void)game_print_fen(game, str);
    print("%s\n", str);
}

static void cmd_echo(void)
{
    char *str;

    if ((str = strtok(NULL, "\n\r")) != NULL) {
        print("%s\n", str);
    }
}

static void cmd_new(void)
{
    stop_thinking();
    game_destroy(game);
    if ((game = game_create()) == NULL) {
        INTERNAL_ERROR();
    }
    set_engine_root_node(current_position());
    computer_side = black;
    set_thinking_done_cb(computer_move);
    unset_search_depth_limit();
    if (!is_xboard) {
        print("New game - computer black\n");
    }
    game_started = false;
    is_force_mode = false;
}

static void cmd_sd(void)
{
    set_search_depth_limit(get_uint(0, MAX_PLY));
}

static void cmd_hint(void)
{
    move m;
    char str[MOVE_STR_BUFFER_LENGTH];

    if (engine_get_best_move(&m) == 0) {
        print_move(current_position(), m, str, horse->move_not, turn());
        print("Hint: %s\n", str);
    }
}

static void cmd_hard(void)
{
    can_ponder = true;
    decide_move();
}

static void cmd_easy(void)
{
    can_ponder = false;
    if (is_comp_turn()) {
        stop_thinking();
    }
}

static void cmd_result(void)
{
    stop_thinking();
    game_started = false;
}

static void cmd_time(void)
{
    set_computer_clock(get_uint(0, UINT_MAX));
}

static void cmd_otim(void)
{
    set_opponent_clock(get_uint(0, UINT_MAX));
}

static void cmd_black(void)
{
    computer_side = black;
    if (is_comp_turn() && !is_force_mode && game_started) {
        decide_move();
    }
}

static void cmd_white(void)
{
    computer_side = white;
    if (is_comp_turn() && !is_force_mode && game_started) {
        decide_move();
    }
}

static void cmd_level(void)
{
    unsigned mps, base, inc;

    mps = get_uint(0, 1024);
    base = get_uint(0, 1024);
    inc = get_uint(0, 1024);
    set_moves_left_in_time(mps);
    set_computer_clock(base * 100);
    set_time_inc(inc * 100);
}

static void cmd_protover(void)
{
    print("feature");
    for (const char **f = features; *f != NULL; ++f) {
        print(" %s", *f);
    }
    print("\nfeature done=1\n");
}

static void cmd_force(void)
{
    stop_thinking();
    is_force_mode = true;
}

static void cmd_go(void)
{
    if (game_started && is_comp_turn()) {
        return;
    }
    if (!is_comp_turn()) {
        computer_side = opponent(computer_side);
    }
    is_force_mode = false;
    game_started = true;
    decide_move();
}

static void cmd_playother(void)
{
    if (!game_started || !is_force_mode || is_comp_turn()) {
        return;
    }
    stop_thinking();
    computer_side = opponent(computer_side);
    decide_move();
}

static void cmd_st(void)
{
    set_secs_per_move(get_uint(1, 0x10000));
}

static void set_var_onoff(bool *variable)
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

static void set_verbosity(void)
{
    set_var_onoff(&verbose);
}

static void set_exitonmove(void)
{
    set_var_onoff(&exit_on_done);
}

static void search_cb(void)
{
    move m;

    if (engine_get_best_move(&m) != 0) {
        // error?
        return;
    }
    print_computer_move(m);
    set_thinking_done_cb(computer_move);
    if (exit_on_done) {
        exit(EXIT_SUCCESS);
    }
}

static void cmd_search(void)
{
    if (game_started) {
        return;
    }
    set_thinking_done_cb(search_cb);
    start_thinking_no_time_limit();
}

static void cmd_analyze(void)
{
    if (game_started) {
        return;
    }
    set_thinking_done_cb(search_cb);
    start_analyze();
}

static void cmd_undo(void)
{
    if (is_force_mode) {
        if (revert() != 0) general_error();
        set_engine_root_node(current_position());
    }
}

static void cmd_redo(void)
{
    if (is_force_mode) {
        if (forward() != 0) general_error();
        set_engine_root_node(current_position());
    }
}

static void cmd_setmovenot(void)
{
    char *str = get_str_arg_lower();

    if (strcmp(str, "coor") == 0) {
        horse->move_not = mn_coordinate;
    }
    else if (strcmp(str, "san") == 0) {
        horse->move_not = mn_san;
    }
    else {
        param_error();
    }
}

static void cmd_getmovenot(void)
{
    switch (horse->move_not) {
        case mn_coordinate:
            print("Using Coordinate move notation\n");
            break;
        case mn_san:
            print("Using Standard algebraic notation\n");
            break;
        default:
            break;
    }
}

static void cmd_set_show_thinking(void)
{
    set_show_thinking(print_current_result);
}

static void cmd_ping(void)
{
    char *str;

    if ((str = get_str_arg_opt()) == NULL) {
        print("pong\n");
        return;
    }
    print("pong %s\n", str);
}

static void cmd_trace(void)
{
    bool on;

    set_var_onoff(&on);
    if (on) {
        errno = 0;
        if (trace_on() != 0) {
            perror("Unable to open log file");
        }
    }
    else {
        trace_off();
    }
}

static void cmd_eval(void)
{
    static char const evaluation_description[] =
" evaluation =\n"
"      material + basic_mobility\n"
"      + middle_game * (pawn_structure + king_fortress + piece_placement)\n"
"      + end_game * passed_pawn_score\n";

    eval_factors ef = compute_eval_factors(current_position());
    print(" material:          %d\n", ef.material);
    print(" middle_game:       %d\n", ef.middle_game);
    print(" end_game:          %d\n", ef.end_game);
    print(" basic_mobility:    %d\n", ef.basic_mobility);
    print(" pawn_structure:    %d\n", ef.pawn_structure);
    print(" passed_pawn_score: %d\n", ef.passed_pawn_score);
    print(" king_fortress:     %d\n", ef.king_fortress);
    print(" piece_placement:   %d\n", ef.piece_placement);
    print("%s  %d\n", evaluation_description, eval(current_position()));
}

static void cmd_pkey(void)
{
    print("%016" PRIx64 "\n",
          position_polyglot_key(current_position(), turn()));
}

static void cmd_getpv(void)
{
}

static void nop(void) {}

static struct cmd_entry cmd_list[] = {
    {"q",            cmd_quit,  NULL},
    {"quit",         cmd_quit,  NULL},
    {"exit",         cmd_quit,  NULL},
    {"perft",        cmd_perft,  NULL},
    {"perfto",       cmd_perfto,  NULL},
    {"perftd",       cmd_perft_distinct,  NULL},
    {"perfts",       cmd_perfts,  NULL},
    {"divide",       cmd_divide,  "depth"},
    {"divideo",      cmd_divideo,  "depth"},
    {"setboard",     cmd_setboard,  "FENSTRING"},
    {"printboard",   cmd_printboard,  NULL},
    {"printfen",     cmd_printfen,  NULL},
    {"echo",         cmd_echo,  NULL},
    {"print",        cmd_echo,  NULL},
    {"getpv",        cmd_getpv,  NULL},
    {"xboard",       set_xboard,  NULL},
    {"new",          cmd_new,  NULL},
    {"protover",     cmd_protover,  NULL},
    {"time",         cmd_time,  NULL},
    {"force",        cmd_force,  NULL},
    {"otim",         cmd_otim,  NULL},
    {"sd",           cmd_sd,  NULL},
    {"go",           cmd_go,  NULL},
    {"result",       cmd_result,  NULL},
    {"hint",         cmd_hint,  NULL},
    {"hard",         cmd_hard,  NULL},
    {"easy",         cmd_easy,  NULL},
    {"post",         cmd_set_show_thinking,  NULL},
    {"nopost",       set_no_show_thinking,  NULL},
    {"level",        cmd_level,  NULL},
    {"black",        cmd_black,  NULL},
    {"white",        cmd_white,  NULL},
    {"playother",    cmd_playother,  NULL},
    {"st",           cmd_st,  NULL},
    {"accepted",     nop,  NULL},
    {"exitonmove",   set_exitonmove,  "on|off"},
    {"random",       nop,  NULL},
    {"rejected",     nop,  NULL},
    {"random",       nop,  NULL},
    {"computer",     nop,  NULL},
    {"name",         nop,  NULL},
    {"search",       cmd_search,  NULL},
    {"analyze",      cmd_analyze,  NULL},
    {"undo",         cmd_undo,  NULL},
    {"redo",         cmd_redo,  NULL},
    {"verbose",      set_verbosity, "on|off"},
    {"setmovenot",   cmd_setmovenot,  "coor|san"},
    {"getmovenot",   cmd_getmovenot,  NULL},
    {"ping",         cmd_ping, NULL},
    {"trace",        cmd_trace, "on|off"},
    {"eval",         cmd_eval, NULL},
    {"polyglot_key", cmd_pkey, NULL}
};

static void init_settings(void)
{
    is_xboard = false;
    exit_on_done = false;
    computer_side = black;
    qsort(cmd_list,
          sizeof cmd_list / sizeof cmd_list[0],
          sizeof *cmd_list,
          (int (*)(const void *, const void *))strcmp);
    game = game_create();
    set_engine_root_node(current_position());
    set_thinking_done_cb(computer_move);
    set_show_thinking(print_current_result);
    set_computer_clock(30000);
    set_opponent_clock(30000);
    set_moves_left_in_time(40);
    set_time_inc(0);
    game_started = false;
}

static void dispatch_command(const char *cmd)
{
    const struct cmd_entry *e;
    char cmdlower[strlen(cmd)+1];

    (void)str_to_lower(strcpy(cmdlower, cmd));
    e = bsearch(cmdlower,
            cmd_list,
            sizeof cmd_list / sizeof cmd_list[0],
            sizeof *cmd_list,
            (int (*)(const void *, const void *))strcmp);
    if (e != NULL) {
        switch (setjmp(command_error)) {
            case 0:
                trace("Command: %s", cmdlower);
                e->cmd_func();
                break;
            case CMD_PARAM_ERROR:
                if (e->paramstr != NULL) {
                    (void)fprintf(stderr,
                                  "Usage: %s %s\n", e->text, e->paramstr);
                }
                break;
            case CMD_GENERAL_ERROR:
                (void)fprintf(stderr, "Unable to do that\n");
                break;
            default:
                INTERNAL_ERROR();
        }
    }
    else {
        (void)fprintf(stderr, "Error: (uknown command): %s\n", cmd);
    }
}

