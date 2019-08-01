override CFLAGS := -std=c99 -pipe -O2 -Wall -Wextra -pedantic $(CFLAGS)

bindir ?= /usr/local/bin

INSTALL ?= install


all: cc1541

cc1541: cc1541.c

test_cc1541: test_cc1541.c

check: cc1541 test_cc1541
	./test_cc1541 ./cc1541

test: check

install: all
	$(INSTALL) -Dpm 0755 ./cc1541 $(DESTDIR)$(bindir)/cc1541

codestyle: cc1541.c test_cc1541.c
	astyle --style=kr -n -s -z2 cc1541.c test_cc1541.c

clean:
	rm -rf *.o *.orig cc1541 test_cc1541 *~

.PHONY: all check clean codestyle install test
