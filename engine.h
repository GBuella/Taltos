
#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>

#include "chess.h"

struct position;

struct engine_result {
    bool first;
    unsigned depth;
    int selective_depth;
    int qdepth;
    move pv[MAX_PLY];
    float value;
    float time_spent;
    uintmax_t node_count;
    const struct hash_table *ht_main;
};

struct hash_table;

void reset_node_counts(void);
uintmax_t get_node_count(void);
int get_fmc_percent(void);
void start_thinking(void);
void start_thinking_no_time_limit(void);
void start_analyze(void);
void stop_thinking(void);
void move_now(void);
void set_clock(unsigned white_time, unsigned black_time);
void set_thinking_done_cb(void (*cb)(void));
void engine_move_count_inc(void);
int engine_get_best_move(move *);
void reset_clock(void);
void set_opponent_clock(unsigned);
void set_computer_clock(unsigned);
void set_search_depth_limit(unsigned);
void set_engine_root_node(const struct position *);
void unset_search_depth_limit(void);
void set_show_thinking(void (*cb)(const struct engine_result));
void set_no_show_thinking(void);
void set_time_inc(unsigned);
void set_moves_left_in_time(unsigned);
void set_secs_per_move(unsigned);
void wait_thinking(void);
int engine_init(void);

#endif
