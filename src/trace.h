
#ifndef TRACE_H
#define TRACE_H

#include <stdarg.h>

#include "macros.h"

struct node;
struct move_fsm;

int trace_on(void);
void trace_off(void);
int log_open(void);
void log_close(void);

#ifdef SEARCH_TRACE_PATH

void trace_node_count_before(struct node *);
void trace_node_count_after(const struct node *);
void trace_node(const struct node *, const struct move_fsm *);
void trace(const char[], ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)))
#endif
;

#else

#define trace_node_count_before(x) ;
#define trace_node_count_after(x) ;
#define trace_node(a, b) ;
#define trace(...) ;

#endif

#endif
