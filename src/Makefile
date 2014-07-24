
.DEFAULT: ;

LDFLAGS=-l pthread

../taltos: main.o chess.o move.o perft.o protocol.o engine.o search.o \
     game.o str_util.o hash.o move_gen.o platform.o eval.o \
     move_order.o trace.o tests.o
	$(CC) $(LDFLAGS) -o $@ *.o

move_gen.o: move_gen_const.inc

move_gen_const.inc: gen_constants
	./gen_constants > $@

gen_constants: gen_constants.c
	$(CC) $(CFLAGS) gen_constants.c -o $@

clean:
	rm -f *.o ../taltos

cleanall:
	rm -rf *.o ../taltos gen_constants move_gen_const.inc gen_constants.dSYM

