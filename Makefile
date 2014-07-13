
SRCS=main.c chess.c move.c perft.c protocol.c engine.c search.c \
     game.c str_util.c hash.c move_gen.c taltos_threads.c eval.c \
     move_order.c trace.c

OBJS=${SRCS:.c=.o}

rel:
	$(MAKE) CFLAGS='-O3 -DSEARCH_TRACE_PATH="taltos.log" -DSLIDING_BYTE_LOOKUP -DUSE_KNIGHT_LOOKUP_TABLE -DNDEBUG -march=native -Wall -Wextra -pedantic -Werror ' \
		TFLAGS='$(TFLAGS)' \
		LDFLAGS='-O3 -march=native -Wall -Werror ' \
		taltos

relvalgrind:
	$(MAKE) CFLAGS='-O3 -DNDEBUG -mpopcnt -Wall -Werror ' \
		TFLAGS='$(TFLAGS)' \
		LDFLAGS='-O3 -mpopcnt -Wall -Werror ' \
		taltos

debug:
	$(MAKE) CFLAGS=' -O0 -DVERIFY_HASH_TABLE -DSEARCH_TRACE_PATH="taltos.log" -DSLIDING_BYTE_LOOKUP -DUSE_KNIGHT_LOOKUP_TABLE  -g -pg -Wall -Wextra -Wpedantic -Werror ' \
		TFLAGS='$(TFLAGS)' \
		LDFLAGS=' -O0 -g -pg -Wall -Werror ' \
		taltos

rellto:
	$(MAKE) CFLAGS=' -O3 -DNDEBUG -flto -march=native -Wall -Werror -mno-vzeroupper ' \
		TFLAGS='$(TFLAGS)' \
		LDFLAGS=' -O3 -flto -march=native -Wall -Werror -mno-vzeroupper -fwhole-program ' \
		taltos

move_gen_const.inc: constants.h gen_constants
	./gen_constants > move_gen_const.inc

gen_constants: gen_constants.c
	$(CC) $(CFLAGS) gen_constants.c -o $@


DEPFILE=.depends

taltos: $(DEPFILE)

.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) $(TFLAGS) -c $<  -o $@

.PHONY: depend clean

taltos: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	$(RM) taltos $(OBJS) gen_constants $(DEPFILE) move_gen_const.inc

depend: $(DEPFILE)

$(DEPFILE):
	touch move_gen_const.inc
	$(CC) -MM $(SRCS) > $(DEPFILE)
	rm move_gen_const.inc

sinclude $(DEPFILE)

