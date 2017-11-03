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

#include "chess.h"
#include "engine.h"
#include "game.h"
#include "search.h"
#include "hash.h"
#include "eval.h"
#include "taltos.h"
#include "move_order.h"
#include "trace.h"

#include <mutex>
#include <thread>
#include <vector>
#include <memory>
#include <cstring>

using std::unique_ptr;
using std::make_unique;
using namespace std::chrono;

namespace taltos
{

// global config
static const struct taltos_conf *horse;

enum player debug_player_to_move;

void
debug_engine_set_player_to_move(enum player p)
{
	debug_player_to_move = p;
}


static std::recursive_mutex engine_mutex;

namespace
{
class engine_guard
{
	std::lock_guard<std::recursive_mutex> guard;

public:
	engine_guard():
		guard(engine_mutex)
	{
	}
};

}


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
static milliseconds base_computer_time;

// Time left on the computer's clock
static milliseconds computer_time;

/*
 * Time added to each player's clock after each move made by that player.
 * Thus computer_time decreases with each move, by the time spent with
 * thinking on that move - but also increases by time_inc each time it
 * is the computer's turn.
 */
static milliseconds time_inc;

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
static const milliseconds time_safety = 30ms;

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

static hash_table *main_tt;

void
set_time_infinite(void)
{
	engine_guard guard;

	trace(__func__);
	time_infinite = true;
}

void
set_search_depth_limit(unsigned limit)
{
	engine_guard guard;
	tracef("%s limit = %u", __func__, limit);

	depth_limit = limit;
}

void
set_search_nps(unsigned rate)
{
	engine_guard guard;
	tracef("%s rate = %u", __func__, rate);

	nps = rate;
}

void
unset_search_depth_limit(void)
{
	engine_guard guard;
	trace(__func__);

	depth_limit = 0;
}

void
set_time_inc(milliseconds n)
{
	engine_guard guard;
	tracef("%s n = %jd", __func__, (intmax_t) n.count());

	stop_thinking();
	is_tc_secs_per_move = false;
	time_infinite = false;
	time_inc = n;
}

void
set_exact_node_count(uintmax_t n)
{
	if (n == 0)
		return;

	engine_guard guard;
	tracef("%s n = %ju", __func__, n);

	stop_thinking();
	exact_node_count = n;
	depth_limit = 0;
	time_infinite = false;
	time_inc = 0ms;
}

void
set_moves_left_in_time(unsigned n)
{
	engine_guard guard;
	tracef("%s n = %u", __func__, n);

	is_tc_secs_per_move = false;
	time_infinite = false;
	moves_left_in_time = n;
	base_moves_per_time = n;
}

void
set_computer_clock(milliseconds t)
{
	engine_guard guard;
	tracef("%s n = %jd", __func__, (intmax_t) t.count());

	computer_time = t;
	time_infinite = false;
}

void
set_secs_per_move(seconds t)
{
	engine_guard guard;
	tracef("%s n = %jd", __func__, (intmax_t) t.count());

	is_tc_secs_per_move = true;
	time_infinite = false;
	computer_time = t;
}

void
set_opponent_clock(milliseconds)
{
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
static move engine_best_move;

/*
 * When thinking is done ( on all threads ) call this function.
 * Used by command_loop.c
 */
static int (*thinking_cb)(uintmax_t);
static uintmax_t thinking_cb_arg;

// Used to keep track of when the current thinking started.
static time_point<steady_clock> thinking_started;

static void (*show_thinking_cb)(const struct engine_result);

struct search_thread_data {
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

	unique_ptr<std::thread> thread;
};

static std::vector<search_thread_data> threads;

static int
board_cmp(const struct position *a, const struct position *b)
{
	return memcmp(a->board, b->board, sizeof(a->board));
}

static std::size_t
filter_duplicates(struct position duplicates[], bool *is_root_duplicate)
{
	trace(__func__);

	std::size_t count = 0;
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
				position_print_fen(history + i, fen, 0, turn);
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

	std::size_t duplicate_count;
	ht_entry entry = ht_set_depth(HT_NULL, 99);
	entry = ht_set_value(entry, vt_exact, 0);
	bool is_root_duplicate;

	duplicate_count = filter_duplicates(duplicates, &is_root_duplicate);
	for (unsigned ti = 0; ti < MAX_THREAD_COUNT; ++ti) {
		struct hash_table *tt = threads[ti].sd.tt;
		if (tt != NULL) {
			if (is_root_duplicate)
				ht_clear(tt);
			for (std::size_t i = 0; i < duplicate_count; ++i)
				ht_pos_insert(tt, duplicates + i, entry);
		}
	}
}

static void
join_all_threads(bool signal_stop)
{
	engine_mutex.lock();
	for (auto& t : threads) {
		if (t.is_started_flag) {
			t.is_started_flag = false;
			(void) signal_stop;
			if (signal_stop)
				t.run_flag = false;
			engine_mutex.unlock();
			trace("thrd_join search thread");
			t.thread->join();
			engine_mutex.lock();
			trace("thrd_join search thread - done");
			t.run_flag = false;
		}
	}
	threads.clear();
	engine_mutex.unlock();
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

	engine_guard guard;

	bool is_valid;

	if (thinking_cb != NULL)
		is_valid = (thinking_cb(thinking_cb_arg) == 0);
	else
		is_valid = false;

	if (is_valid && !is_tc_secs_per_move && !time_infinite) {
		auto t = duration_cast<milliseconds>(steady_clock::now() - thinking_started);

		if (t > computer_time)
			computer_time = 0ms;
		else
			computer_time -= t;

		tracef("%s time_spent = %" PRIdMAX " computer_time = %" PRIdMAX,
		    __func__, (intmax_t) t.count(), (intmax_t) computer_time.count());
	}
}

int
engine_get_best_move(move *m)
{
	engine_guard guard;

	if (engine_best_move != 0) {
		*m = engine_best_move;
		return 0;
	}
	else {
		return -1;
	}
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
	engine_guard guard;

	show_thinking_cb = cb;
}

void
set_no_show_thinking(void)
{
	engine_guard guard;

	show_thinking_cb = NULL;
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
	dst->time_spent =
	    duration_cast<milliseconds>(steady_clock::now() - data->sd.thinking_started);
	dst->ht_usage =
	    (int)(ht_usage(data->sd.tt) * 1000 / ht_slot_count(data->sd.tt));

	memcpy(dst->pv, src->pv, sizeof(dst->pv));
	dst->pv[data->sd.depth / PLY + 1] = 0;
}

ht_entry
engine_current_entry(void)
{
	if (threads[0].sd.tt == NULL)
		return HT_NULL;
	return ht_lookup_deep(threads[0].sd.tt,
	    history + history_length - 1, 1, max_value);
}

ht_entry
engine_get_entry(const struct position *pos)
{
	if (main_tt == NULL)
		return HT_NULL;
	return ht_lookup_deep(main_tt, pos, 1, max_value);
}

static void
setup_search(struct search_thread_data *thread)
{
	ht_entry entry;

	thread->sd.depth = PLY;
	entry = ht_lookup_deep(thread->sd.tt, &thread->root, 1, max_value);
	if (ht_value_type(entry) == vt_exact && ht_depth(entry) > 0) {
		if (ht_value(entry) == 0 && ht_depth(entry) == 99)
			return;

		thread->sd.depth = ht_depth(entry) + 1;
		if (thread->export_best_move && ht_has_move(entry))
			engine_best_move = ht_move(entry);
	}
}

static void
iterative_deepening(struct search_thread_data *data)
{
	trace(__func__);

	struct engine_result engine_result;

	engine_mutex.lock();

	memset(&engine_result, 0, sizeof(engine_result));
	engine_result.first = true;

	setup_search(data);
	while ((data->sd.depth_limit == -1 && data->sd.depth < MAX_PLY)
	    || (data->sd.depth <= data->sd.depth_limit * PLY)) {
		struct search_result result;

		engine_mutex.unlock();
		tracef("iterative_deepening -- start depth %d", data->sd.depth);
		result = search(&data->root, debug_player_to_move,
		    data->sd, &data->run_flag, engine_result.pv);
		update_engine_result(data, &engine_result, &result);
		tracef("iterative_deepening -- done depth %d", data->sd.depth);
		engine_mutex.lock();
		if (result.is_terminated)
			break;
		if (data->sd.node_count_limit > 0)
			data->sd.node_count_limit -= result.node_count;
		engine_result.depth = data->sd.depth / PLY;
		if (data->export_best_move && result.best_move != 0)
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
	engine_mutex.unlock();
	tracef("%s -- done", __func__);
	// data->is_started_flag = false; ??
}

static milliseconds
get_time_for_move(void)
{
	milliseconds result;

	if (is_tc_secs_per_move)
		result = computer_time;
	else if (moves_left_in_time > 0)
		result = computer_time / moves_left_in_time;
	else
		result = computer_time / default_move_divisor;

	if (result > time_safety)
		result -= time_safety;
	else
		result = 1ms;

	tracef("%s result = %ju", __func__, (intmax_t) result.count());

	return result;
}

static void
think(bool infinite, bool single_thread)
{
	(void) single_thread; // ignored, no paralell search implemented yet

	engine_guard guard;
	trace(__func__);

	stop_thinking();

	assert(threads.size() == 0);
	threads.resize(1);

	thinking_started = threads[0].sd.thinking_started = steady_clock::now();

	ht_swap(main_tt);
	move_order_swap_history();

	threads[0].sd.tt = main_tt;
	if (infinite || depth_limit == 0)
		threads[0].sd.depth_limit = -1;
	else
		threads[0].sd.depth_limit = depth_limit;

	if (exact_node_count != 0) {
		threads[0].sd.time_limit = 0ms;
		threads[0].sd.node_count_limit = exact_node_count;
	}
	else if (infinite || time_infinite) {
		threads[0].sd.time_limit = 0ms;
		threads[0].sd.node_count_limit = 0;
	}
	else {
		if (nps == 0) {
			threads[0].sd.time_limit = get_time_for_move();
			threads[0].sd.node_count_limit = 0;
		}
		else {
			threads[0].sd.time_limit = 0ms;
			threads[0].sd.node_count_limit =
			    (nps * (intmax_t)get_time_for_move().count()) / 1000;
		}
	}

	tracef("%s time_limit: %ju node_count_limit: %ju",
	    __func__, (intmax_t) threads[0].sd.time_limit.count(),
	    threads[0].sd.node_count_limit);

	horse->mutex.lock();

	ht_resize_mb(threads[0].sd.tt, horse->hash_table_size_mb);

	horse->mutex.unlock();

	threads[0].thinking_cb = &thinking_done;
	threads[0].show_thinking_cb = show_thinking_cb;
	threads[0].root = history[history_length - 1];
	threads[0].is_started_flag = true;
	threads[0].export_best_move = true;
	threads[0].sd.settings = horse->search;
	threads[0].run_flag = true;
	threads[0].thread = make_unique<std::thread>(iterative_deepening, &threads[0]);
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
	engine_guard guard;

	thinking_cb = cb;
	thinking_cb_arg = arg;
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

void
engine_process_move(move m)
{
	engine_guard guard;
	trace(__func__);

	// todo: rotate_threads();
	struct position *pos = history + history_length;
	position_make_move(pos, pos - 1, m);
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
}

std::size_t
engine_ht_size(void)
{
	engine_guard guard;
	trace(__func__);

	struct hash_table *tt = threads[0].sd.tt;

	if (tt == nullptr)
		return 0;
	else
		return ht_size(tt);
}

void
reset_engine(const struct position *pos)
{
	engine_guard guard;
	trace(__func__);

	stop_thinking();
	horse->mutex.lock();
	if (main_tt != nullptr)
		ht_destroy(main_tt);
	main_tt = ht_create_mb(horse->hash_table_size_mb);
	horse->mutex.unlock();
	history[0] = *pos;
	history_length = 1;
	fill_best_move();
}

void
engine_conf_change(void)
{
	unsigned new_hash_size;

	horse->mutex.lock();
	new_hash_size = horse->hash_table_size_mb;
	horse->mutex.unlock();

	engine_guard guard;

	if (main_tt == nullptr)
		main_tt = ht_create_mb(new_hash_size);
	else
		ht_resize_mb(main_tt, new_hash_size);
}


void
init_engine(const struct taltos_conf *h)
{
	trace(__func__);

	struct position pos;

	horse = h;

	position_read_fen(&pos, start_position_fen, NULL, NULL);
	reset_engine(&pos);
	depth_limit = 0;
}

}
