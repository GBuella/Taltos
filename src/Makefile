
.DEFAULT: ;

UTIL_DIR=../util

../taltos: main.o chess.o move.o perft.o protocol.o engine.o search.o \
     game.o str_util.o hash.o move_gen.o platform.o eval.o timers.o \
     move_order.o trace.o tests.o polyglotbook.o fen_book.o book.o
	$(CC) $(LDFLAGS) -o $@ *.o

move_gen.o: move_gen_const.inc

move_gen_const.inc: $(UTIL_DIR)/gen_constants
	$(UTIL_DIR)/gen_constants > $@

clean:
	rm -f *.o ../taltos

cleanall:
	rm -rf *.o ../taltos move_gen_const.inc

sinclude depend
