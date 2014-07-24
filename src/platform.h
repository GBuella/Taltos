
#ifndef PLATFORM_H
#define PLATFORM_H

typedef void *thread_pointer;

void init_threading(void);
thread_pointer thread_allocate(void);
void thread_create(thread_pointer, void (*entry)(void*), void *arg);
void search_thread_cancel_point(void);
void thread_exit(void);
void thread_kill(thread_pointer);
void thread_join(thread_pointer);
void set_timer(unsigned centi_seconds);
void set_timer_cb(void (*func)(void *), void *arg);
void cancel_timer(void);
unsigned get_timer(void);

#endif
