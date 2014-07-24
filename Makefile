
prod:
	CFLAGS=`./util/genCFLAGS prod` $(MAKE) -C src

debug:
	CFLAGS=`./util/genCFLAGS debug` $(MAKE) -C src

clean:
	$(MAKE) -C src clean
	rm -f tests/.out tests/.time

cleanall:
	rm -f tests/.out tests/.time
	$(MAKE) -C src cleanall

test:
	$(MAKE) -C tests tests

alltests:
	$(MAKE) -C tests alltests

