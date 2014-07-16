
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include "macros.h"
#include "protocol.h"
#include "chess.h"
#include "engine.h"
#include "hash.h"
#include "taltos.h"
#include "trace.h"

void mount_horse(struct taltos_conf *horse)
{
    horse->move_not = mn_san;
}

void process_args(int argc, char **arg, struct taltos_conf *horse UNUSED)
{
    int arg_i;

    arg_i = 1;
    ++arg;
    while (arg_i < argc) {
        ++arg_i;
        ++arg;
    }
}

int main(int argc, char **argv)
{
    struct taltos_conf horse;

    mount_horse(&horse);
    process_args(argc, argv, &horse);
#   if !defined(WIN32) && !defined(WIN64)
    setvbuf(stdout, NULL, _IOLBF, 0x1000);
#   endif /* !WIN */

    init_move_gen();
    if (engine_init() != 0) return EXIT_FAILURE;
    loop_cli(&horse);
    log_close();
    return EXIT_SUCCESS;
}
