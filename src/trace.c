
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "search.h"
#include "trace.h"

#ifdef SEARCH_TRACE_PATH

static FILE *logfile = NULL;
static bool is_trace_on = true;

int trace_on(void)
{
    if (log_open() != 0) return -1;
    is_trace_on = true;
    return 0;
}

void trace_off(void)
{
    log_close();
    is_trace_on = false;
}

int log_open(void)
{
    if (logfile != NULL) return 0;
    logfile = fopen(STR(SEARCH_TRACE_PATH), "w");
    return logfile == NULL;
}

void log_close(void)
{
    if (logfile != NULL) {
        fclose(logfile);
        logfile = NULL;
    }
}

void trace_node(const struct node *node, const struct move_fsm *ml)
{
    if (logfile == NULL || !is_trace_on) return;

    char str[BOARD_BUFFER_LENGTH + MOVE_STR_BUFFER_LENGTH];

    fprintf(logfile, "Node count: %" PRIuMAX " %" PRIuMAX "\n",
            node->node_count_pivot, get_node_count());
    board_print(str, node->pos, white);
    fwrite(str, strlen(str), 1, logfile);
    fprintf(logfile, "Depth: %d alpha: %d beta: %d\n",
            node->depth, node->alpha, node->beta);
    position_print_fen(node->pos, str, 1, 1, white);
    fprintf(logfile, "%s\n", str);
    print_move(node->pos, node->current_move, str, mn_san, white);
    fprintf(logfile, "At move: %s #%u\n", str, ml->legal_counter);
    fprintf(logfile, "--------------------------\n\n");
}

void trace_node_count_before(struct node *node)
{
    if (logfile == NULL || !is_trace_on) return;
    node->node_count_pivot = get_node_count();
}

void trace_node_count_after(const struct node *node)
{
    if (logfile == NULL || !is_trace_on) return;
    if (node->root_distance < 20) return;

    const struct node *n = node;
    player turn = white;

    while (!n->is_search_root) {
        --n;
        if (node->current_move == 0) return;
    }
    fprintf(logfile, "Node count: ");
    do {
        char str[MOVE_STR_BUFFER_LENGTH];

        print_move(n->pos, n->current_move, str, mn_san, turn);
        fprintf(logfile, "%s ", str);
        ++n;
        turn = opponent(turn);
    } while (n <= node);
    fprintf(logfile, " -- d: %d", node[1].depth);
    fprintf(logfile, " a: %d b: %d", node->alpha, node->beta);
    fprintf(logfile, " %" PRIuMAX "\n",
                get_node_count() - node->node_count_pivot);
}

void trace(const char format[], ...)
{
    if (logfile == NULL || !is_trace_on) return;

    size_t l = strlen(format);
    va_list args;
    va_start(args, format);
    vfprintf(logfile, format, args);
    va_end(args);
    if (format[l - 1] != '\n') {
        fputc('\n', logfile);
    }
}

#else

int trace_on(void) { return 0; }

void trace_off(void) { }

int log_open(void) { return 0; }

void log_close(void) { }

#endif
