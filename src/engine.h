/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#ifndef TALTOS_ENGINE_H
#define TALTOS_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#include "search.h"

struct position;
struct taltos_conf;
struct hash_table;

struct engine_result {
	bool first;
	unsigned depth;
	struct search_result sresult;
	move pv[MAX_PLY];
	int ht_usage;
	uintmax_t time_spent;
};

void start_thinking(void);
void start_thinking_infinite(void);
void start_thinking_single_thread(void);
void stop_thinking(void);
void move_now(void);
void set_clock(unsigned white_time, unsigned black_time);
void set_thinking_done_cb(int (*cb)(uintmax_t), uintmax_t arg);
void engine_move_count_inc(void);
int engine_get_best_move(move*);
void reset_clock(void);
void set_opponent_clock(unsigned);
void set_computer_clock(unsigned);
void set_search_depth_limit(unsigned);
void set_search_nps(unsigned);
ht_entry engine_current_entry(void);
ht_entry engine_get_entry(const struct position*);
void unset_search_depth_limit(void);
void set_show_thinking(void (*cb)(const struct engine_result));
void set_no_show_thinking(void);
void set_time_inc(unsigned);
void set_moves_left_in_time(unsigned);
void set_secs_per_move(unsigned);
void set_time_infinite(void);
void wait_thinking(void);
void init_engine(const struct taltos_conf*);
void reset_engine(const struct position*);
void engine_process_move(move);
void debug_engine_set_player_to_move(enum player);
size_t engine_ht_size(void);
void engine_conf_change(void);
void set_exact_node_count(uintmax_t);

#endif
