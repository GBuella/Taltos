
.DEFAULT: ;

UTIL_DIR=../util

../taltos: main.o chess.o move.o perft.o protocol.o engine.o search.o \
     game.o str_util.o hash.o move_gen.o platform.o eval.o \
     move_order.o trace.o tests.o
	$(CC) $(LDFLAGS) -o $@ *.o

move_gen.o: move_gen_const.inc

move_gen_const.inc: $(UTIL_DIR)/gen_constants
	$(UTIL_DIR)/gen_constants > $@

clean:
	rm -f *.o ../taltos

cleanall:
	rm -rf *.o ../taltos move_gen_const.inc


chess.o: chess.c chess.h constants.h position.h macros.h bitmanipulate.h \
 hash.h str_util.h
engine.o: engine.c platform.h chess.h engine.h search.h position.h \
 macros.h bitmanipulate.h constants.h hash.h trace.h eval.h taltos.h \
 game.h
eval.o: eval.c bitmanipulate.h macros.h position.h constants.h chess.h \
 hash.h search.h engine.h eval.h
game.o: game.c chess.h game.h
hash.o: hash.c macros.h hash.h chess.h position.h bitmanipulate.h \
 constants.h z_random.inc search.h engine.h trace.h
main.o: main.c macros.h chess.h engine.h hash.h taltos.h game.h trace.h \
 platform.h
move.o: move.c macros.h bitmanipulate.h constants.h chess.h position.h \
 hash.h str_util.h
move_gen.o: move_gen.c macros.h bitmanipulate.h chess.h position.h \
 constants.h hash.h move_gen_const.inc
move_order.o: move_order.c search.h chess.h engine.h position.h macros.h \
 bitmanipulate.h constants.h hash.h eval.h
perft.o: perft.c perft.h chess.h position.h macros.h bitmanipulate.h \
 constants.h hash.h engine.h search.h
platform.o: platform.c macros.h platform_posix.inc platform.h
protocol.o: protocol.c perft.h chess.h game.h engine.h taltos.h hash.h \
 trace.h macros.h eval.h
search.o: search.c macros.h search.h chess.h engine.h position.h \
 bitmanipulate.h constants.h hash.h platform.h trace.h eval.h
str_util.o: str_util.c str_util.h chess.h
tests.o: tests.c str_util.h chess.h perft.h search.h engine.h position.h \
 macros.h bitmanipulate.h constants.h hash.h eval.h game.h
trace.o: trace.c search.h chess.h engine.h position.h macros.h \
 bitmanipulate.h constants.h hash.h trace.h
