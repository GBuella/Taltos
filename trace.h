
#ifndef TRACE_H
#define TRACE_H

#include <stdarg.h>

#include "macros.h"

struct node;
struct move_fsm;

#ifdef SEARCH_TRACE_PATH

int trace_on(void);
void trace_off(void);
int log_open(void);
void log_close(void);
void trace_node_count_before(struct node *);
void trace_node_count_after(const struct node *);
void trace_node(const struct node *, const struct move_fsm *);
void trace(const char[], ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)))
#endif
;

#else

static inline int trace_on(void) { return 0; }
static inline void trace_off(void) {}
static inline int log_open(void) { return 0; }
static inline void log_close(void) {}
static inline void
trace_node_count_before(const struct node *node UNUSED)
{}
static inline void
trace_node_count_after(const struct node *a UNUSED) {}
static inline void trace_node(const void *a UNUSED, const void *b UNUSED)
{}
static inline void trace(const char[], ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)))
#endif
;
static inline void trace(const char a[] UNUSED, ...) {}

#endif

#endif
