override CFLAGS := -std=c99 -O2 $(CFLAGS)

bindir ?= /usr/local/bin

INSTALL ?= install


all: cc1541

cc1541: cc1541.o

test_cc1541: test_cc1541.o

check: cc1541 test_cc1541
	./test_cc1541 ./cc1541

test: check

install: all
	$(INSTALL) -Dpm 0755 ./cc1541 $(DESTDIR)$(bindir)/cc1541

clean:
	rm -rf *.o cc1541 test_cc1541 *~

.PHONY: all check clean install test
