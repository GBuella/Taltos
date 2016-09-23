
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <threads.h>

#include "chess.h"
#include "engine.h"
#include "game.h"
#include "search.h"
#include "hash.h"
#include "eval.h"
#include "taltos.h"
#include "move_order.h"
#include "trace.h"

// global config
static const struct taltos_conf *horse;

static mtx_t engine_mutex;


/*	Timing	*/

/*
 * Two timing modes are supported:
 * A fix number of seconds per each move ( when is_tc_secs_per_move is true ),
 * and conventinal clock mode.
 * These are based on the description of the xboard level and st commands at:
 * http://www.gnu.org/software/xboard/engine-intf.html#11
 */
static bool is_tc_secs_per_move;

static bool time_infinite;

// Starting time on each players clock
static unsigned base_computer_time;

// Time left on the computer's clock
static unsigned computer_time;

/*
 * Time added to each player's clock after each move made by that player.
 * Thus computer_time decreases with each move, by the time spent with
 * thinking on that move - but also increases by time_inc each time it
 * is the computer's turn.
 */
static unsigned time_inc;

/*
 * If moves_left_in_time is greater than zero, it means the clock is
 * going to have a significant amount of time added after the specified
 * number of moves. Thus it is a good idea to allocate
 * 'computer_time divided by moves_left_in_time' time for the next move.
 *
 * If moves_left_in_time is zero, it means the game is lost when
 * computer_time reaches zero.
 */
static unsigned moves_left_in_time;

/*
 * When moves_left_in_time is zero, use this default_move_divisor
 * as a guide for dividing the game time.
 * I.e.: if the engine has 300 seconds on its clock, and
 * default_move_divisor is 50, then use 300/50 == 6 seconds for
 * thinking on the next move to make.
 */
static const unsigned default_move_divisor = 40;

/*
 * Timing is reset after each base_moves_per_time moves.
 * Zero is a special value, it means clocks won't be reset, the
 * whole game must be played in the time given by starting values
 * of clocks ( plus per-move-increments if any ).
 */
static unsigned base_moves_per_time = 0;

/*
 * A safety margin. Every time the time to spend on thinking on a
 * particular move is computed, this number of centiseconds are
 * substracted from the result. This should help avoid running out of
 * time when something goes wrong.
 */
static const unsigned time_safety = 3;

/*
 * Two ways of measuring "time" are supported:
 * The default is using a monotonic clock provided by the platform.
 * When nps is not zero, it is interpreted as the search speed in
 * nodes-per-second, and search time computed by dividing the node count
 * with nps. Thus a time limit of T seconds, is essentially converted to
 * a maximum node count of T*nps available for search.
 */
static unsigned nps = 0;

/*
 * If depth_limit is non-zero, the thinking stops upon reaching the specified
 * depth, even if there is plenty time left.
 */
static int depth_limit = 0;

void
set_time_infinite(void)
{
	mtx_lock(&engine_mutex);
	trace(__func__);
	time_infinite = true;
	mtx_unlock(&engine_mutex);
}

void
set_search_depth_limit(unsigned limit)
{
	mtx_lock(&engine_mutex);
	tracef("%s limit = %u", __func__, limit);
	depth_limit = limit;
	mtx_unlock(&engine_mutex);
}

void
set_search_nps(unsigned rate)
{
	mtx_lock(&engine_mutex);
	tracef("%s rate = %u", __func__, rate);
	nps = rate;
	mtx_unlock(&engine_mutex);
}

void
unset_search_depth_limit(void)
{
	mtx_lock(&engine_mutex);
	trace(__func__);
	depth_limit = 0;
	mtx_unlock(&engine_mutex);
}

void
set_time_inc(unsigned n)
{
	mtx_lock(&engine_mutex);
	tracef("%s n = %u", __func__, n);
	stop_thinking();
	is_tc_secs_per_move = false;
	time_infinite = false;
	time_inc = n;
	mtx_unlock(&engine_mutex);
}

void
set_moves_left_in_time(unsigned n)
{
	mtx_lock(&engine_mutex);
	tracef("%s n = %u", __func__, n);
	is_tc_secs_per_move = false;
	time_infinite = false;
	moves_left_in_time = n;
	base_moves_per_time = n;
	mtx_unlock(&engine_mutex);
}

void
set_computer_clock(unsigned t)
{
	mtx_lock(&engine_mutex);
	tracef("%s n = %u", __func__, t);
	computer_time = t;
	time_infinite = false;
	mtx_unlock(&engine_mutex);
}

void
set_secs_per_move(unsigned t)
{
	mtx_lock(&engine_mutex);
	tracef("%s n = %u", __func__, t);
	is_tc_secs_per_move = true;
	time_infinite = false;
	computer_time = t * 100;
	mtx_unlock(&engine_mutex);
}

void
set_opponent_clock(unsigned t)
{
	(void) t;
	// We don't care about the other player's clock for now
}


/*	Threads	*/

/*
 * All positions since the last irreversible move.
 * Needed for three-fold repetition detection.
 */
static struct position history[51];
static unsigned history_length;

/*
 * The best move known by the engine in the current position.
 * Before search, it is at least filled with the first known legal move.
 */
static move engine_best_move;

/*
 * When thinking is done ( on all threads ) call this function.
 * Used by command_loop.c
 */
static int (*thinking_cb)(uintmax_t);
static uintmax_t thinking_cb_arg;

// Used to keep track of when the current thinking started.
static uintmax_t thinking_started;

static void (*show_thinking_cb)(const struct engine_result);
static unsigned thread_count = 1;

struct search_thread_data {
	mtx_t mutex;
	thrd_t thr;

	/*
	 * The run_flag is checked by the search thread to see it
	 * still needs to run, and cleared by the engine
	 * to signal to the search thread that it needs to stop.
	 */
	atomic_flag run_flag;

	/*
	 * The is_started_flag is checked by the engine.
	 * It is set by the engine upon starting a thread,
	 * and cleared by the engine upon joining a thread.
	 */
	bool is_started_flag;

	struct position root;
	struct search_description sd;
	bool export_best_move;
	void (*thinking_cb)(void);
	void (*show_thinking_cb)(const struct engine_result);
};

static struct search_thread_data threads[MAX_THREAD_COUNT];

static int
board_cmp(const struct position *a, const struct position *b)
{
	return memcmp(a->board, b->board, sizeof(a->board));
}

static size_t
filter_duplicates(struct position duplicates[])
{
	size_t count = 0;
	for (unsigned i = 0; i < history_length - 1; ++i) {
		for (unsigned j = i + 1; j < history_length; ++j) {
			if (board_cmp(history + i, history + j) == 0) {
				duplicates[count] = history[i];
				break;
			}
		}
	}
	return count;
}

static void
add_history_as_repetition(void)
{
	struct position duplicates[ARRAY_LENGTH(history)];
	size_t duplicate_count;
	ht_entry entry = ht_set_depth(HT_NULL, 99);
	entry = ht_set_value(entry, vt_exact, 0);

	duplicate_count = filter_duplicates(duplicates);
	for (unsigned ti = 0; ti < MAX_THREAD_COUNT; ++ti) {
		struct hash_table *tt = threads[ti].sd.tt;
		if (tt != NULL)
			for (size_t i = 0; i < duplicate_count; ++i)
				ht_pos_insert(tt, duplicates + i, entry);
	}
}

static void
join_all_threads(bool signal_stop)
{
	for (unsigned i = 0; i < MAX_THREAD_COUNT; ++i) {
		struct search_thread_data *thread = threads + i;
		mtx_lock(&(thread->mutex));
		if (thread->is_started_flag) {
			thread->is_started_flag = false;
			if (signal_stop)
				atomic_flag_clear(&(thread->run_flag));
			mtx_unlock(&(thread->mutex));
			tracef("thrd_join search thread #%u", i);
			thrd_join(thread->thr, NULL);
			tracef("thrd_join search thread #%u - done", i);
			if (!signal_stop)
				atomic_flag_clear(&(thread->run_flag));
		}
		else {
			mtx_unlock(&(thread->mutex));
		}
	}
}

void
stop_thinking(void)
{
	trace(__func__);
	join_all_threads(true);
}

void
wait_thinking(void)
{
	trace(__func__);
	join_all_threads(false);
}

static void
thinking_done(void)
{
	trace(__func__);

	bool is_valid;

	mtx_lock(&engine_mutex);

	if (thinking_cb != NULL)
		is_valid = (thinking_cb(thinking_cb_arg) == 0);
	else
		is_valid = false;

	if (is_valid && !is_tc_secs_per_move && !time_infinite) {
		uintmax_t time_spent = xseconds_since(thinking_started);

		if (time_spent > computer_time)
			computer_time = 0;
		else
			computer_time -= (unsigned)time_spent;

		tracef("%s time_spent = %" PRIuMAX " computer_time = %u",
		    __func__, time_spent, computer_time);
	}

	mtx_unlock(&engine_mutex);
}

int
engine_get_best_move(move *m)
{
	int result = 0;

	mtx_lock(&engine_mutex);

	if (engine_best_move != 0)
		*m = engine_best_move;
	else
		result = -1;

	mtx_unlock(&engine_mutex);
	return result;
}

static void
fill_best_move(void)
{
	move moves[MOVE_ARRAY_LENGTH];

	if (gen_moves(history + history_length - 1, moves) != 0) {
		engine_best_move = moves[0];
	}
	else {
		engine_best_move = 0;
	}
}

void
set_show_thinking(void (*cb)(const struct engine_result))
{
	mtx_lock(&engine_mutex);

	show_thinking_cb = cb;

	mtx_unlock(&engine_mutex);
}

void
set_no_show_thinking(void)
{
	mtx_lock(&engine_mutex);

	show_thinking_cb = NULL;

	mtx_unlock(&engine_mutex);
}

static void
update_engine_result(const struct search_thread_data *data,
		struct engine_result *dst,
		const struct search_result *src)
{
	uintmax_t node_count_sum = dst->sresult.node_count + src->node_count;
	dst->sresult = *src;
	dst->sresult.node_count = node_count_sum;
	dst->time_spent = xseconds_since(data->sd.thinking_started);
	dst->ht_usage =
	    (int)(ht_usage(data->sd.tt) * 1000 / ht_slot_count(data->sd.tt));
	dst->pv[0] = dst->sresult.best_move = src->best_move;

	struct position child;
	position_make_move(&child, &data->root, dst->pv[0]);
	ht_extract_pv(data->sd.tt, &child,
	    data->sd.depth - PLY,
	    &dst->pv[1], -src->value);
}

ht_entry
engine_current_entry(void)
{
	if (threads[0].sd.tt == NULL)
		return HT_NULL;
	return ht_lookup_deep(threads[0].sd.tt,
	    history + history_length - 1, 1, max_value);
}

static void
setup_search(struct search_thread_data *thread)
{
	ht_entry entry;

	entry = ht_lookup_deep(thread->sd.tt, &thread->root, 1, max_value);
	if (ht_value_type(entry) == vt_exact && ht_depth(entry) > 0) {
		thread->sd.depth = ht_depth(entry) + 1;
		if (thread->export_best_move && ht_has_move(entry))
			engine_best_move = ht_move(entry);
	}
	else {
		thread->sd.depth = 1;
	}
}

static int
iterative_deepening(void *arg)
{
	trace(__func__);
	assert(arg != NULL);

	struct engine_result engine_result;
	struct search_thread_data *data = (struct search_thread_data*)arg;

	mtx_lock(&(data->mutex));

	memset(&engine_result, 0, sizeof(engine_result));
	engine_result.first = true;

	setup_search(data);
	while ((data->sd.depth_limit == -1 && data->sd.depth < MAX_PLY)
	    || (data->sd.depth <= data->sd.depth_limit * PLY)) {
		struct search_result result;

		mtx_unlock(&(data->mutex));
		tracef("iterative_deepening -- start depth %d", data->sd.depth);
		result = search(&data->root, data->sd, &data->run_flag);
		tracef("iterative_deepening -- done depth %d", data->sd.depth);
		mtx_lock(&(data->mutex));
		if (result.is_terminated)
			break;
		if (data->sd.node_count_limit > 0)
			data->sd.node_count_limit -= result.node_count;
		engine_result.depth = data->sd.depth / PLY;
		update_engine_result(data, &engine_result, &result);
		if (data->export_best_move && result.best_move != 0)
			engine_best_move = result.best_move;
		if (data->show_thinking_cb != NULL) {
			data->show_thinking_cb(engine_result);
			engine_result.first = false;
		}
		if (abs(result.value) >= mate_value)
			break;
		data->sd.depth++;
		// data->sd.depth += PLY;
	}
	if (data->thinking_cb != NULL)
		data->thinking_cb();
	mtx_unlock(&(data->mutex));
	tracef("%s -- done", __func__);
	// data->is_started_flag = false;

	return 0;
}

static unsigned
get_time_for_move(void)
{
	unsigned result;

	if (is_tc_secs_per_move)
		result = computer_time;
	else if (moves_left_in_time > 0)
		result = computer_time / moves_left_in_time;
	else
		result = computer_time / default_move_divisor;

	if (result > time_safety)
		result -= time_safety;
	else
		result = 1;

	tracef("%s result = %u", __func__, result);

	return result;
}

static void
think(bool infinite, bool single_thread)
{
	(void) single_thread; // ignored, no paralell search implemented yet

	mtx_lock(&engine_mutex);

	stop_thinking();

	mtx_lock(&threads[0].mutex);

	thinking_started = threads[0].sd.thinking_started = xnow();

	ht_swap(threads[0].sd.tt);

	if (infinite || depth_limit == 0)
		threads[0].sd.depth_limit = -1;
	else
		threads[0].sd.depth_limit = depth_limit;

	if (infinite || time_infinite) {
		threads[0].sd.time_limit = 0;
		threads[0].sd.node_count_limit = 0;
	}
	else {
		threads[0].sd.time_limit = get_time_for_move();
		threads[0].sd.node_count_limit = (nps * get_time_for_move()) / 100;
	}

	threads[0].thinking_cb = &thinking_done;
	threads[0].show_thinking_cb = show_thinking_cb;
	threads[0].root = history[history_length - 1];
	threads[0].is_started_flag = true;
	threads[0].export_best_move = true;
	(void) atomic_flag_test_and_set(&(threads[0].run_flag));

	thrd_create(&threads[0].thr, iterative_deepening, &threads[0]);

	mtx_unlock(&threads[0].mutex);
	mtx_unlock(&engine_mutex);
}

void
start_thinking_infinite(void)
{
	think(true, false);
}

void
start_thinking_single_thread(void)
{
	think(false, true);
}

void
start_thinking(void)
{
	think(false, false);
}

void
set_thinking_done_cb(int (*cb)(uintmax_t), uintmax_t arg)
{
	mtx_lock(&engine_mutex);

	thinking_cb = cb;
	thinking_cb_arg = arg;

	mtx_unlock(&engine_mutex);
}

/*
 * Adjust timing after a move on the computer's side:
 * If it is fix timing per move, nothing to do.
 * Otherwise, make note of how many moves are left till clock
 */
void
engine_move_count_inc(void)
{
	trace(__func__);

	if (is_tc_secs_per_move)
		return;

	if (moves_left_in_time > 0) {
		--moves_left_in_time;
	}
	else {
		if (base_moves_per_time > 0) {
			moves_left_in_time = base_moves_per_time;
			computer_time += base_computer_time;
		}
		else {
			/* ?? */
		}
	}
}

static void
xmtx_init(mtx_t *mtx, int type)
{
	if (mtx_init(mtx, type) != thrd_success) {
		fputs("init_engine error", stderr);
		abort();
	}
}

void
engine_process_move(move m)
{
	trace(__func__);

	mtx_lock(&engine_mutex);

	// todo: rotate_threads();
	struct position *pos = history + history_length;
	position_make_move(pos, pos - 1, m);
	if (is_move_irreversible(pos - 1, m)) {
		history_length = 1;
		history[0] = *pos;
	}
	else {
		++history_length;
	}
	add_history_as_repetition();
	fill_best_move();

	mtx_unlock(&engine_mutex);
}

void
reset_engine(const struct position *pos)
{
	trace(__func__);

	mtx_lock(&engine_mutex);

	stop_thinking();
	for (unsigned i = 0; i < thread_count; ++i) {
		if (threads[i].sd.tt != NULL) {
			ht_clear(threads[i].sd.tt);
			continue;
		}
		threads[i].sd.tt = ht_create(horse->main_hash_size);
		if (threads[i].sd.tt == NULL) {
			fprintf(stderr,
			    "Unable to allocate transposition "
			    "table - with 2^%u buckets",
			    horse->main_hash_size);
			exit(EXIT_FAILURE);
		}
	}
	for (size_t i = thread_count; i < MAX_THREAD_COUNT; ++i) {
		if (threads[i].sd.tt != NULL) {
			ht_destroy(threads[i].sd.tt);
			threads[i].sd.tt = NULL;
		}
	}
	history[0] = *pos;
	history_length = 1;
	fill_best_move();

	mtx_unlock(&engine_mutex);
}

void
init_engine(const struct taltos_conf *h)
{
	struct position pos;
	xmtx_init(&engine_mutex, mtx_plain | mtx_recursive);
	for (size_t i = 0; i < sizeof(threads) / sizeof(threads[0]); ++i)
		xmtx_init(&(threads[i].mutex), mtx_plain);

	horse = h;

	position_read_fen(&pos, start_position_fen, NULL, NULL);
	reset_engine(&pos);
	depth_limit = 0;
}
