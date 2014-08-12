
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "platform.h"
#include "chess.h"
#include "engine.h"
#include "search.h"
#include "hash.h"
#include "trace.h"
#include "eval.h"
#include "taltos.h"

static const struct taltos_conf *horse;
static struct position *root = NULL;
static thread_pointer thinking_thread;
static move engine_best_move;
static void (*thinking_cb)(void);
static unsigned computer_time;
static unsigned opponent_time;
static unsigned time_inc;
static unsigned moves_left_in_time;
static unsigned base_computer_time;
static unsigned base_opponent_time;
static unsigned base_moves_per_time = 0;
static int depth_limit;
static void (*show_thinking_cb)(const struct engine_result);
static int is_tc_secs_per_move;
static unsigned timer_current_csecs = 0;

static struct search_description sd_search;
static struct search_description sd_analyze;

void set_search_depth_limit(unsigned limit)
{
    depth_limit = limit;
}

void unset_search_depth_limit(void)
{
    depth_limit = -1;
}

void set_time_inc(unsigned n)
{
    stop_thinking();
    is_tc_secs_per_move = 0;
    time_inc = n;
}

void set_moves_left_in_time(unsigned n)
{
    cancel_timer();
    is_tc_secs_per_move = 0;
    moves_left_in_time = n;
    base_moves_per_time = n;
}

void set_computer_clock(unsigned t)
{
    cancel_timer();
    computer_time = t;
}

void set_secs_per_move(unsigned t)
{
    cancel_timer();
    is_tc_secs_per_move = 1;
    computer_time = t * 100;
}

void set_opponent_clock(unsigned t)
{
    opponent_time = t;
}

void stop_thinking(void)
{
    trace("Stopping timer");
    cancel_timer();
    trace("Stopping thinking thread");
    thread_kill(thinking_thread);
}

void wait_thinking(void)
{
    thread_join(thinking_thread);
}

int engine_get_best_move(move *m)
{
    if (engine_best_move != 0) {
        *m = engine_best_move;
        return 0;
    }
    else {
        return -1;
    }
}

static void search_time_end(void *arg UNUSED)
{
    (void)arg;
    trace("search_time_end called\n");
    (*thinking_cb)();
}

static void fill_best_move(void)
{
    move moves[MOVE_ARRAY_LENGTH];

    memset(moves, 0, sizeof moves);
    if (gen_moves(root, moves) != moves) {
        engine_best_move = moves[0];
    }
    else {
        engine_best_move = 0;
    }
}

void set_engine_root_node(const struct position *pos)
{
    position_copy(root, pos);
    fill_best_move();
}

void set_show_thinking(void (*cb)(const struct engine_result))
{
    show_thinking_cb = cb;
}

void set_no_show_thinking(void)
{
    show_thinking_cb = NULL;
}

static void
show_thinking(const struct search_description sd, int value,
              bool first, int selective_depth, int qdepth)
{
    struct engine_result result;

    ht_extract_pv(sd.ht_main, root, sd.depth, result.pv);
    result.first = first;
    result.depth = sd.depth / PLY;
    result.value = ((float)value) / PAWN_VALUE;
    result.ht_main = sd.ht_main;
    result.qdepth = qdepth;
    if (selective_depth > 0) {
        result.selective_depth = selective_depth / PLY;
    }
    else {
        result.selective_depth = 0;
    }
    if (sd.uses_timer) {
        result.time_spent = (float)(get_timer()) / 100;
    }
    else {
        result.time_spent = (float)(clock() - sd.thinking_started);
        if (CLOCKS_PER_SEC > 1) {
            result.time_spent /= CLOCKS_PER_SEC;
        }
    }
    result.node_count = get_node_count();
    (*show_thinking_cb)(result);
}

static void iterative_deepening(void *arg)
{
    assert(arg != NULL);

    move best_move;
    int value;
    struct search_description sd;
    bool is_first_result = true;

    memcpy(&sd, arg, sizeof sd);
    sd.depth = 0;
    while ((sd.depth_limit == -1) || (sd.depth <= (int)sd.depth_limit * PLY)) {
        int selective_depth, qdepth;

        value = search(root, &best_move, sd, &selective_depth, &qdepth);
        if (best_move != 0) {
            engine_best_move = best_move;
            if (show_thinking_cb != NULL) {
                show_thinking(sd, value, is_first_result,
                                selective_depth, qdepth);
                is_first_result = false;
            }
        }
        if (value >= MATE_VALUE || value <= -MATE_VALUE) {
            break;
        }
        sd.depth++;
        //sd.depth += PLY;
    }
    (*thinking_cb)();
}

static unsigned get_time_for_move(void)
{
    if (is_tc_secs_per_move) {
        return computer_time;
    }
    if (moves_left_in_time) {
        return (computer_time / moves_left_in_time) - 1;
    }
    return computer_time / 10;
}

static void start_thinking_one_thread(struct search_description *sd)
{
    reset_node_counts();
    thread_kill(thinking_thread);
    sd_search.depth_limit = depth_limit;
    sd_search.thinking_started = clock();
    sd_search.uses_timer = false;
    thread_create(thinking_thread, iterative_deepening, sd);
}

static void setup_search_hashes(void)
{
    sd_search.ht_main = ht_resize(sd_search.ht_main,
                                  horse->main_hash_size,
                                  true, 4);
    sd_search.ht_aux = ht_resize(sd_search.ht_aux,
                                 horse->aux_hash_size,
                                 false, 1);
}

static void setup_analyze_hashes(void)
{
    sd_analyze.ht_main = ht_resize(sd_analyze.ht_main,
                                   horse->main_hash_size,
                                   true, 4);
    sd_analyze.ht_aux = ht_resize(sd_analyze.ht_aux,
                                  horse->aux_hash_size,
                                  false, 1);
}

void start_thinking_no_time_limit(void)
{
    setup_search_hashes();
    start_thinking_one_thread(&sd_search);
}

void start_analyze(void)
{
    setup_analyze_hashes();
    start_thinking_one_thread(&sd_analyze);
}

void start_thinking(void)
{
    setup_search_hashes();
    ht_swap(sd_search.ht_main);
    thread_kill(thinking_thread);
    reset_node_counts();
    sd_search.thinking_started = clock();
    sd_search.depth_limit = (depth_limit == 0) ? -1 : depth_limit;
    sd_search.uses_timer = true;
    set_timer_cb(search_time_end, NULL);
    timer_current_csecs = get_time_for_move();
    thread_create(thinking_thread, iterative_deepening, &sd_search);
    set_timer(timer_current_csecs);
}

void set_thinking_done_cb(void (*cb)(void))
{
    thinking_cb = cb;
}

void engine_move_count_inc(void)
{
    if (moves_left_in_time > 0) {
        --moves_left_in_time;
    }
    else {
        if (base_moves_per_time > 0) {
            moves_left_in_time = base_moves_per_time;
            computer_time += base_computer_time;
            opponent_time += base_opponent_time;
        }
        else {
            /* ?? */
        }
    }
}

void init_engine(const struct taltos_conf *h)
{
    horse = h;
    root = position_create();
    if (root == NULL) {
        exit(EXIT_FAILURE);
    }
    if ((thinking_thread = thread_allocate()) == NULL) {
        exit(EXIT_FAILURE);
    }
    sd_search.ht_main = NULL;
    sd_search.ht_aux = NULL;
    sd_search.strict_repetitions = false;
    sd_search.lmr_factor = 2*PLY + PLY/2;
    sd_search.nmr_factor = 2*PLY;
    sd_search.lmr_factor = 0;
    //sd_search.nmr_factor = 0;
    sd_search.twp = false;
    sd_analyze.threat_extension = true;
    sd_analyze.ht_main = NULL;
    sd_analyze.ht_aux = NULL;
    sd_analyze.strict_repetitions = true;
    sd_analyze.lmr_factor = 0;
    sd_analyze.nmr_factor = 0;
    sd_analyze.twp = false;
    sd_analyze.threat_extension = false;
    depth_limit = 0;
}

