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

wrap: README.md
	fold -s -w 70 < README.md | \
	  perl -pe 's/[\t\040]+$$//' > README.md.T
	mv -f README.md.T README.md

clean:
	rm -rf README.md.T *.o *.orig cc1541 test_cc1541 *~

.PHONY: all check clean codestyle install test wrap
