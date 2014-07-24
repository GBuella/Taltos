
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include "macros.h"
#include "chess.h"
#include "engine.h"
#include "hash.h"
#include "taltos.h"
#include "trace.h"
#include "platform.h"

void run_internal_tests(void);

static struct taltos_conf horse = {
    mn_san, /* default move notation for printing move
               it is reset to coordination notation in xboard mode */
    false,  /* do not print CPU time at exit by default */
    0,
    22,     /* default main hash table size ( 2^23 entries )
               - entries overwritten only with entries with greater depth */
    21,     /* default auxiliary hash table size
               - entries always overwritten */
    22,     /* default size of hash table used during strict analyze search
               - entries overwritten only with entries with greater depth */
    21      /* default auxiliary hash table size for analyze search
               - entries always overwritten */
};

static void onexit(void)
{
    if (horse.timing) {
        float t = (float)(clock() - horse.start_time);
        fprintf(stderr, "%.2f\n", t / CLOCKS_PER_SEC);
    }
    log_close();
}

void process_args(int argc, char **arg)
{
    int arg_i;

    arg_i = 1;
    ++arg;
    while (arg_i < argc) {
        if (strcmp(*arg, "-t") == 0) {
            horse.timing = true;
            horse.start_time = clock();
        }
        ++arg_i;
        ++arg;
    }
}

int main(int argc, char **argv)
{
    init_move_gen();
    run_internal_tests();
    init_threading();
    init_engine(&horse);
    process_args(argc, argv);
    atexit(onexit);
#   if !defined(WIN32) && !defined(WIN64)
    setvbuf(stdout, NULL, _IOLBF, 0x1000);
#   endif /* !WIN */
    loop_cli(&horse);
    return EXIT_SUCCESS;
}
