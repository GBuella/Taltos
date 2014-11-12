
#include <errno.h>
#include <stdbool.h>
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

static const char *progname;
static bool must_run_internal_tests = true;
static struct taltos_conf horse;
static void onexit(void);
static void usage(int status);
static void process_args(char **arg);
static void init_book(struct book **book);
static void setup_defaults(void);

int main(int argc, char **argv)
{
    struct book *book;

    (void)argc;
    setup_defaults();
    init_move_gen();
    init_threading();
    process_args(argv);
    init_book(&book);
    init_engine(&horse);
    if (must_run_internal_tests) {
        run_internal_tests();
    }
    if (atexit(onexit) != 0) {
        return EXIT_FAILURE;
    }
#   if !defined(WIN32) && !defined(WIN64)
    (void)setvbuf(stdout, NULL, _IOLBF, 0x1000);
#   endif /* !WIN */

    loop_cli(&horse, book);

#if __STDC_VERSION__ < 201112L
    /* no "noreturn" before C11 */
    return EXIT_SUCCESS;
#endif

}

static void setup_defaults(void)
{
    horse.move_not = mn_san;
            /* default move notation for printing move
               it is reset to coordination notation in xboard mode
            */

    horse.timing = false;
            /* do not print CPU time at exit by default,
               can be set using the -t command line argument
            */
    horse.main_hash_size = 22;
            /* default main hash table size ( 2^23 entries )
                    - entries overwritten only with
                      entries with greater depth
            */
    horse.aux_hash_size = 21;
            /* default auxiliary hash table size
                    - entries always overwritten */
    horse.analyze_hash_size = 22;
            /* default size of hash table used
                during strict analyze search
                - entries overwritten only with
                  entries with greater depth
            */
    horse.analyze_aux_hash_size = 21;
            /* default auxiliary hash table size for analyze search
                    - entries always overwritten
            */
    horse.book_path = NULL;   /* book path, none by default */
    horse.book_type = bt_builtin;  /* use the builtin book by default */
    horse.use_unicode = false;
}

static void onexit(void)
{
    if (horse.timing) {
        float t = (float)(clock() - horse.start_time);
        (void)fprintf(stderr, "%.2f\n", t / CLOCKS_PER_SEC);
    }
    log_close();
}

static char *argdup(char *arg)
{
    char *p = malloc(strlen(arg) + 1);

    if (p == NULL) abort();
    strcpy(p, arg);
    return p;
}

static void process_args(char **arg)
{
    progname = *arg;
    for (++arg; *arg != NULL; ++arg) {
        if (strcmp(*arg, "-t") == 0) {
            horse.timing = true;
            horse.start_time = clock();
        }
        else if (strcmp(*arg, "--notest") == 0) {
            must_run_internal_tests = false;
        }
        else if (strcmp(*arg, "--book") == 0
                 || strcmp(*arg, "--fenbook") == 0)
        {
            if (arg[1] == NULL || horse.book_type != bt_builtin) {
                usage(EXIT_FAILURE);
            }
            if (strcmp(*arg, "--book") == 0) {
                horse.book_type = bt_polyglot;
            }
            else {
                horse.book_type = bt_fen;
            }
            ++arg;
            horse.book_path = argdup(*arg);
        }
        else if (strcmp(*arg, "--nobook") == 0) {
            if (horse.book_type != bt_builtin) {
                usage(EXIT_FAILURE);
            }
            horse.book_type = bt_empty;
        }
        else if (strcmp(*arg, "--unicode") == 0) {
            horse.use_unicode = true;
        }
        else if (strcmp(*arg, "--help") == 0 || strcmp(*arg, "-h") == 0) {
            usage(EXIT_SUCCESS);
        }
    }
}

static void usage(int status)
{
    fprintf((status == EXIT_SUCCESS) ? stdout : stderr,
        "taltos chess engine\n"
        "usage: %s blabla\n",
        progname);
    exit(status);
}

static void init_book(struct book **book)
{
    errno = 0;

    *book = book_open(horse.book_type, horse.book_path);

    if (*book == NULL) {
        if (errno != 0 && horse.book_path != NULL) {
            perror(horse.book_path);
        }
        exit(EXIT_FAILURE);
    }
}

