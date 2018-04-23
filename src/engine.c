/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2018, Gabor Buella
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
#include "tt.h"
#include "eval.h"
#include "taltos.h"
#include "move_order.h"
#include "trace.h"

// global config
static const struct taltos_conf *horse;

enum player debug_player_to_move;

void
debug_engine_set_player_to_move(enum player p)
{
	debug_player_to_move = p;
}

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
static uintmax_t nps = 0;

/*
 * If depth_limit is non-zero, the thinking stops upon reaching the specified
 * depth, even if there is plenty time left.
 */
static int depth_limit = 0;

/*
 * If exact_node_count is non-zero, the thinking searches exactly this
 * number of nodes.
 */
static uintmax_t exact_node_count = 0;

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
set_exact_node_count(uintmax_t n)
{
	if (n == 0)
		return;

	mtx_lock(&engine_mutex);
	tracef("%s n = %ju", __func__, n);
	stop_thinking();
	exact_node_count = n;
	depth_limit = 0;
	time_infinite = false;
	time_inc = 0;
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
static struct position history[1024];
static unsigned history_length;

/*
 * The best move known by the engine in the current position.
 * Before search, it is at least filled with the first known legal move.
 */
static struct move engine_best_move;

/*
 * When thinking is done ( on all threads ) call this function.
 * Used by command_loop.c
 */
static int (*thinking_cb)(uintmax_t);
static uintmax_t thinking_cb_arg;

// Used to keep track of when the current thinking started.
static taltos_systime thinking_started;

static void (*show_thinking_cb)(const struct engine_result);
static unsigned thread_count = 1;

struct search_thread_data {
	thrd_t thr;

	/*
	 * The run_flag is checked by the search thread to see it
	 * still needs to run, and cleared by the engine
	 * to signal to the search thread that it needs to stop.
	 */
	bool run_flag;

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
filter_duplicates(struct position duplicates[], bool *is_root_duplicate)
{
	trace(__func__);

	size_t count = 0;
	enum player turn = debug_player_to_move;
	*is_root_duplicate = false;

	if ((history_length % 2) == 0)
		turn = opponent_of(turn);

	for (unsigned i = 0; i < history_length - 1; ++i) {
		for (unsigned j = i + 1; j < history_length; ++j) {
			if ((i - j) % 2 == 0
			    && board_cmp(history + i, history + j) == 0) {
				if (j == history_length - 1)
					*is_root_duplicate = true;

				char fen[0x1000];
				position_print_fen(fen, history + i);
				tracef("Duplicate found: %s", fen);

				duplicates[count++] = history[i];
				break;
			}
		}
		turn = opponent_of(turn);
	}
	return count;
}

static void
add_history_as_repetition(void)
{
	trace(__func__);

	static struct position duplicates[ARRAY_LENGTH(history)];

	size_t duplicate_count;
	bool is_root_duplicate;

	duplicate_count = filter_duplicates(duplicates, &is_root_duplicate);
	for (unsigned ti = 0; ti < MAX_THREAD_COUNT; ++ti) {
		struct tt *tt = threads[ti].sd.tt;
		if (tt == NULL)
			continue;

		if (is_root_duplicate)
			tt_clear(tt);
		for (size_t i = 0; i < duplicate_count; ++i)
			tt_insert_exact_value(tt, duplicates[i].zhash, 0,
					      tt_max_depth, true);
	}
}

static void
join_all_threads(bool signal_stop)
{
	for (unsigned i = 0; i < MAX_THREAD_COUNT; ++i) {
		struct search_thread_data *thread = threads + i;
		mtx_lock(&engine_mutex);
		if (thread->is_started_flag) {
			thread->is_started_flag = false;
			(void) signal_stop;
			if (signal_stop)
				thread->run_flag = false;
			mtx_unlock(&engine_mutex);
			tracef("thrd_join search thread #%u", i);
			thrd_join(thread->thr, NULL);
			tracef("thrd_join search thread #%u - done", i);
			thread->run_flag = false;
		}
		else {
			mtx_unlock(&engine_mutex);
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
engine_get_best_move(struct move *m)
{
	int result = 0;

	mtx_lock(&engine_mutex);

	if (!is_null_move(engine_best_move))
		*m = engine_best_move;
	else
		result = -1;

	mtx_unlock(&engine_mutex);
	return result;
}

static void
fill_best_move(void)
{
	struct move moves[MOVE_ARRAY_LENGTH];

	if (gen_moves(history + history_length - 1, moves) != 0) {
		engine_best_move = moves[0];
	}
	else {
		engine_best_move = null_move();
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
	uintmax_t qnode_count_sum = dst->sresult.qnode_count + src->qnode_count;
	dst->sresult = *src;
	dst->sresult.node_count = node_count_sum;
	dst->sresult.qnode_count = qnode_count_sum;
	dst->time_spent = xseconds_since(data->sd.thinking_started);
	dst->tt_usage =
	    (int)(tt_usage(data->sd.tt) * 1000 / tt_slot_count(data->sd.tt));

	memcpy(dst->pv, src->pv, sizeof(dst->pv));
	dst->pv[data->sd.depth / PLY + 1] = null_move();
}

struct tt_lookup_result
engine_current_position_in_tt(void)
{
	return engine_lookup_position(history + history_length - 1);
}

struct tt_lookup_result
engine_lookup_position(const struct position *pos)
{
	struct tt_lookup_result result = {0, };

	if (threads[0].sd.tt == NULL)
		tt_lookup(threads[0].sd.tt, pos->zhash,
			  0, &result);

	return result;
}

static void
setup_search(struct search_thread_data *thread)
{
	struct tt_lookup_result lr;

	tt_lookup(thread->sd.tt, thread->root.zhash, 0, &lr);
	if (!lr.found_exact_value)
		return;

	if (lr.value == 0 && lr.depth == tt_max_depth)
		return;

	thread->sd.depth = lr.depth + 1;
	if (thread->export_best_move && lr.move_count > 0) {
		struct move moves[MOVE_ARRAY_LENGTH];
		unsigned move_count = gen_moves(&thread->root, moves);
		if (move_count > lr.moves[0])
			engine_best_move = moves[lr.moves[0]];
	}
}

static int
iterative_deepening(void *arg)
{
	trace(__func__);
	assert(arg != NULL);

	struct engine_result engine_result;
	struct search_thread_data *data = (struct search_thread_data*)arg;

	mtx_lock(&engine_mutex);

	memset(&engine_result, 0, sizeof(engine_result));
	engine_result.first = true;

	setup_search(data);
	while ((data->sd.depth_limit == -1 && data->sd.depth < MAX_PLY)
	    || (data->sd.depth <= data->sd.depth_limit * PLY)) {
		struct search_result result;

		mtx_unlock(&engine_mutex);
		tracef("iterative_deepening -- start depth %d", data->sd.depth);
		tt_generation_step(data->sd.tt);
		result = search(&data->root, &data->sd,
				&data->run_flag, engine_result.pv);
		update_engine_result(data, &engine_result, &result);
		tracef("iterative_deepening -- done depth %d", data->sd.depth);
		mtx_lock(&engine_mutex);
		if (result.is_terminated)
			break;
		if (data->sd.node_count_limit > 0)
			data->sd.node_count_limit -= result.node_count;
		engine_result.depth = data->sd.depth / PLY;
		if (data->export_best_move && !is_null_move(result.best_move))
			engine_best_move = result.best_move;
		if (data->show_thinking_cb != NULL) {
			data->show_thinking_cb(engine_result);
			engine_result.first = false;
		}
		if (abs(result.value) >= mate_value)
			break;
		data->sd.depth++;
	}

	if (engine_result.sresult.node_count <= UINT_MAX) {
		tracef("repro: nodes %u\n",
		    (unsigned)engine_result.sresult.node_count);
		trace("repro: search_sync");
	}
	else {
		trace("repro: nodes -- too many");
	}

	if (data->thinking_cb != NULL)
		data->thinking_cb();
	mtx_unlock(&engine_mutex);
	tracef("%s -- done", __func__);
	// data->is_started_flag = false; ??

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

	trace(__func__);

	mtx_lock(&engine_mutex);

	stop_thinking();

	thinking_started = threads[0].sd.thinking_started = xnow();

	move_order_swap_history();

	if (infinite || depth_limit == 0)
		threads[0].sd.depth_limit = -1;
	else
		threads[0].sd.depth_limit = depth_limit;

	if (exact_node_count != 0) {
		threads[0].sd.time_limit = 0;
		threads[0].sd.node_count_limit = exact_node_count;
	}
	else if (infinite || time_infinite) {
		threads[0].sd.time_limit = 0;
		threads[0].sd.node_count_limit = 0;
	}
	else {
		if (nps == 0) {
			threads[0].sd.time_limit = get_time_for_move();
			threads[0].sd.node_count_limit = 0;
		}
		else {
			threads[0].sd.time_limit = 0;
			threads[0].sd.node_count_limit =
			    (nps * get_time_for_move()) / 100;
		}
	}

	tracef("%s time_limit: %" PRIuMAX " node_count_limit: %" PRIuMAX,
	    __func__, threads[0].sd.time_limit, threads[0].sd.node_count_limit);

	mtx_lock(horse->mutex);

	struct tt *new_tt =
	    tt_resize_mb(threads[0].sd.tt, horse->hash_table_size_mb);

	mtx_unlock(horse->mutex);

	if (new_tt != NULL)
		threads[0].sd.tt = new_tt;

	threads[0].thinking_cb = &thinking_done;
	threads[0].show_thinking_cb = show_thinking_cb;
	threads[0].root = history[history_length - 1];
	threads[0].is_started_flag = true;
	threads[0].export_best_move = true;
	threads[0].sd.settings = horse->search;
	threads[0].run_flag = true;

	thrd_create(&threads[0].thr, iterative_deepening, &threads[0]);

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
	unsigned before = moves_left_in_time;

	if (is_tc_secs_per_move)
		return;

	if (moves_left_in_time > 1) {
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

	tracef("%s before: %u after: %u", __func__, before, moves_left_in_time);
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
engine_process_move(struct move m)
{
	trace(__func__);

	mtx_lock(&engine_mutex);

	// todo: rotate_threads();
	struct position *pos = history + history_length;
	make_move(pos, pos - 1, m);
	if (is_move_irreversible(pos - 1, m)) {
		history_length = 1;
		history[0] = *pos;
	}
	else {
		++history_length;
		if (history_length > ARRAY_LENGTH(history)) {
			tracef("%s history_length > ARRAY_LENGTH(history)",
			    __func__);
			abort();
		}
	}
	add_history_as_repetition();
	fill_best_move();

	mtx_unlock(&engine_mutex);
}

size_t
engine_ht_size(void)
{
	trace(__func__);

	size_t size;

	mtx_lock(&engine_mutex);

	struct tt *tt = threads[0].sd.tt;

	if (tt == NULL)
		size = 0;
	else
		size = tt_size(tt);

	mtx_unlock(&engine_mutex);

	return size;
}

void
reset_engine(const struct position *pos)
{
	trace(__func__);

	mtx_lock(&engine_mutex);

	stop_thinking();
	for (unsigned i = 0; i < thread_count; ++i) {
		if (threads[i].sd.tt != NULL) {
			tt_clear(threads[i].sd.tt);
			continue;
		}
		threads[i].sd.tt = tt_create_mb(horse->hash_table_size_mb);
		if (threads[i].sd.tt == NULL) {
			fprintf(stderr,
			    "Unable to allocate transposition "
			    "table - with size %umb",
			    horse->hash_table_size_mb);
			exit(EXIT_FAILURE);
		}
	}
	for (size_t i = thread_count; i < MAX_THREAD_COUNT; ++i) {
		if (threads[i].sd.tt != NULL) {
			tt_destroy(threads[i].sd.tt);
			threads[i].sd.tt = NULL;
		}
	}
	history[0] = *pos;
	history_length = 1;
	fill_best_move();

	mtx_unlock(&engine_mutex);
}

void
engine_conf_change(void)
{
	unsigned new_hash_size;

	mtx_lock(horse->mutex);
	new_hash_size = horse->hash_table_size_mb;
	mtx_unlock(horse->mutex);

	for (unsigned i = 0; i < MAX_THREAD_COUNT; ++i) {
		struct search_thread_data *thread = threads + i;
		mtx_lock(&engine_mutex);
		if (!thread->is_started_flag) {
			struct tt *new_tt =
			    tt_resize_mb(threads[0].sd.tt, new_hash_size);
			if (new_tt != NULL)
				thread->sd.tt = new_tt;
		}
		mtx_unlock(&engine_mutex);
	}
}


void
init_engine(const struct taltos_conf *h)
{
	trace(__func__);

	struct position pos;
	xmtx_init(&engine_mutex, mtx_plain | mtx_recursive);

	horse = h;

	position_read_fen(start_position_fen, &pos);
	reset_engine(&pos);
	depth_limit = 0;
}
