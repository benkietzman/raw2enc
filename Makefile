# raw2enc
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2024-01-04
# copyright  : kietzman.org
# email      : ben@kietzman.org

prefix=/usr/local

all: bin/passthru

bin/passthru: ../common/libcommon.a obj/passthru.o bin
	g++ -o bin/passthru obj/passthru.o $(LDFLAGS) -L../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lssl -ltar -lz

bin:
	if [ ! -d bin ]; then mkdir bin; fi;

../common/libcommon.a: ../common/Makefile
	cd ../common; make;

../common/Makefile: ../common/configure
	cd ../common; ./configure;

../common/configure:
	cd ../: git clone https://github.com/benkietzman/common.git

obj/passthru.o: passthru.cpp ../common/Makefile obj
	g++ -ggdb -Wall -c passthru.cpp -o obj/passthru.o $(CPPFLAGS) -I../common

obj:
	if [ ! -d obj ]; then mkdir obj; fi;

install: bin/passthru $(prefix)/raw2enc
	install --mode=775 bin/passthru $(prefix)/raw2enc/passthru_preload

$(prefix)/raw2enc:
	mkdir $(prefix)/raw2enc

clean:
	-rm -fr obj bin

uninstall:
	-rm -fr $(prefix)/raw2enc
