
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

static const struct taltos_conf *horse;
static struct position history[51];
static unsigned history_length;
static move engine_best_move;
static void (*thinking_cb)(void);
static unsigned computer_time;
static unsigned opponent_time;
static unsigned time_inc;
static unsigned moves_left_in_time;
static unsigned base_computer_time;
static unsigned base_opponent_time;
static unsigned base_moves_per_time = 0;
static unsigned nps = 0;
static int depth_limit;
static void (*show_thinking_cb)(const struct engine_result);
static bool is_tc_secs_per_move;
static unsigned thread_count = 1;

static mtx_t engine_mutex;

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

void
set_search_depth_limit(unsigned limit)
{
	mtx_lock(&engine_mutex);
	depth_limit = limit;
	mtx_unlock(&engine_mutex);
}

void
set_search_nps(unsigned rate)
{
	mtx_lock(&engine_mutex);
	nps = rate;
	mtx_unlock(&engine_mutex);
}

void
unset_search_depth_limit(void)
{
	mtx_lock(&engine_mutex);
	depth_limit = -1;
	mtx_unlock(&engine_mutex);
}

void
set_time_inc(unsigned n)
{
	mtx_lock(&engine_mutex);
	stop_thinking();
	is_tc_secs_per_move = false;
	time_inc = n;
	mtx_unlock(&engine_mutex);
}

void
set_moves_left_in_time(unsigned n)
{
	mtx_lock(&engine_mutex);
	is_tc_secs_per_move = false;
	moves_left_in_time = n;
	base_moves_per_time = n;
	mtx_unlock(&engine_mutex);
}

void
set_computer_clock(unsigned t)
{
	mtx_lock(&engine_mutex);
	computer_time = t;
	mtx_unlock(&engine_mutex);
}

void
set_secs_per_move(unsigned t)
{
	mtx_lock(&engine_mutex);
	is_tc_secs_per_move = true;
	computer_time = t * 100;
	depth_limit = 0;
	mtx_unlock(&engine_mutex);
}

void
set_opponent_clock(unsigned t)
{
	mtx_lock(&engine_mutex);
	opponent_time = t;
	mtx_unlock(&engine_mutex);
}

static void
join_all_threads(bool signal_stop)
{
	if (signal_stop)
		mtx_lock(&engine_mutex);
	for (unsigned i = 0; i < MAX_THREAD_COUNT; ++i) {
		struct search_thread_data *thread = threads + i;
		mtx_lock(&(thread->mutex));
		if (thread->is_started_flag) {
			thread->is_started_flag = false;
			if (signal_stop)
				atomic_flag_clear(&(thread->run_flag));
			mtx_unlock(&(thread->mutex));
			thrd_join(thread->thr, NULL);
			if (!signal_stop)
				atomic_flag_clear(&(thread->run_flag));
		}
		else {
			mtx_unlock(&(thread->mutex));
		}
	}
	if (signal_stop)
		mtx_unlock(&engine_mutex);
}

void
stop_thinking(void)
{
	join_all_threads(true);
}

void
wait_thinking(void)
{
	join_all_threads(false);
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
		result = search(&data->root, data->sd, &data->run_flag);
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
	// data->is_started_flag = false;

	return 0;
}

static unsigned
get_time_for_move(void)
{
	if (is_tc_secs_per_move)
		return computer_time;
	if (moves_left_in_time)
		return (computer_time / moves_left_in_time) - 1;
	return computer_time / 10;
}

static void
start_thinking_one_thread(void)
{
	mtx_lock(&engine_mutex);
	stop_thinking();
	if (depth_limit == 0)
		threads[0].sd.depth_limit = -1;
	else
		threads[0].sd.depth_limit = depth_limit;
	threads[0].sd.thinking_started = xnow();
	if (is_tc_secs_per_move)
		threads[0].sd.node_count_limit
		    = nps * (get_time_for_move() / 100);
	else
		threads[0].sd.node_count_limit = 0;
	threads[0].sd.time_limit = 0;
	threads[0].thinking_cb = thinking_cb;
	threads[0].show_thinking_cb = show_thinking_cb;
	threads[0].root = history[history_length - 1];
	threads[0].is_started_flag = true;
	threads[0].export_best_move = true;
	(void) atomic_flag_test_and_set(&(threads[0].run_flag));
	thrd_create(&threads[0].thr,
	    iterative_deepening,
	    &threads[0]);
	mtx_unlock(&engine_mutex);
}

void
start_thinking_no_time_limit(void)
{
	start_thinking_one_thread();
}

void
start_thinking(void)
{
	mtx_lock(&engine_mutex);
	stop_thinking();
	ht_swap(threads[0].sd.tt);
	threads[0].sd.thinking_started = xnow();
	if (depth_limit == 0)
		threads[0].sd.depth_limit = -1;
	else
		threads[0].sd.depth_limit = depth_limit;
	threads[0].sd.time_limit = get_time_for_move();
	threads[0].sd.node_count_limit = nps * (get_time_for_move() / 100);
	threads[0].thinking_cb = thinking_cb;
	threads[0].show_thinking_cb = show_thinking_cb;
	threads[0].root = history[history_length - 1];
	threads[0].is_started_flag = true;
	threads[0].export_best_move = true;
	(void) atomic_flag_test_and_set(&(threads[0].run_flag));
	thrd_create(&threads[0].thr,
	    iterative_deepening,
	    &threads[0]);
	mtx_unlock(&engine_mutex);
}

void
set_thinking_done_cb(void (*cb)(void))
{
	thinking_cb = cb;
}

void
engine_move_count_inc(void)
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
