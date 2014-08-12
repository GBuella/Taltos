
UTIL_DIR=./util

prod: util/gen_constants
	CFLAGS=`$(UTIL_DIR)/genCFLAGS.sh prod '$(CFLAGS)'` \
	LDFLAGS=`$(UTIL_DIR)/genLDFLAGS.sh prod '$(LDFLAGS)'` \
	$(MAKE) -C src

debug: util/gen_constants
	CFLAGS=`$(UTIL_DIR)/genCFLAGS.sh debug '$(CFLAGS)'` \
	LDFLAGS=`$(UTIL_DIR)/genLDFLAGS.sh debug '$(LDFLAGS)'` \
	$(MAKE) -C src

for_prof: util/gen_constants
	CFLAGS=`$(UTIL_DIR)/genCFLAGS.sh gprof '$(CFLAGS)'` \
	LDFLAGS=`$(UTIL_DIR)/genLDFLAGS.sh gprof '$(LDFLAGS)'` \
	$(MAKE) -C src

gprofuse: util/gen_constants
	CFLAGS=`$(UTIL_DIR)/genCFLAGS.sh gprofuse '$(CFLAGS)'` \
	LDFLAGS=`$(UTIL_DIR)/genLDFLAGS.sh gprofuse '$(LDFLAGS)'` \
	$(MAKE) -C src

util/gen_constants: util/gen_constants.c
	CFLAGS=`$(UTIL_DIR)/genCFLAGS.sh prod '$(CFLAGS)'` \
	LDFLAGS=`$(UTIL_DIR)/genLDFLAGS.sh prod '$(LDFLAGS)'` \
	$(MAKE) -C util gen_constants

clean:
	$(MAKE) -C src clean
	rm -f tests/.out tests/.time gmon.out

cleanall:
	rm -f tests/.out tests/.time gmon.out
	$(MAKE) -C src cleanall
	$(MAKE) -C util cleanall

test:
	$(MAKE) -C tests tests

alltests:
	$(MAKE) -C tests alltests

