CC1541_CFLAGS = -std=c99 -pipe -O2 -Wall -Wextra -pedantic

ifeq ($(DISABLE_WERROR),)
CC1541_CFLAGS += -Werror
endif

override CFLAGS := $(CC1541_CFLAGS) $(CFLAGS)

bindir ?= /usr/local/bin

INSTALL ?= install

VERSION := $(shell grep 'define VERSION' cc1541.c | cut -d\" -f2)

CC1541_SRC := Makefile $(wildcard *.c *.h *.sln *.vcxproj* LICENSE* README*)

all: cc1541

cc1541: cc1541.c

test_cc1541: test_cc1541.c

check: cc1541 test_cc1541
	./test_cc1541 ./cc1541

test: check

install: all
	$(INSTALL) -Dpm 0755 ./cc1541 $(DESTDIR)$(bindir)/cc1541

cc1541-$(VERSION).tar: clean $(CC1541_SRC)
	mkdir -p cc1541-$(VERSION)
	cp -a $(CC1541_SRC) cc1541-$(VERSION)/
	chmod 0644 cc1541-$(VERSION)/*
	tar cf cc1541-$(VERSION).tar cc1541-$(VERSION)/
	rm -rf cc1541-$(VERSION)/

dist-bz2: cc1541-$(VERSION).tar
	bzip2 -9kz cc1541-$(VERSION).tar

dist-gz: cc1541-$(VERSION).tar
	gzip -9k cc1541-$(VERSION).tar

dist-xz: cc1541-$(VERSION).tar
	xz -ek cc1541-$(VERSION).tar

dist: dist-gz
	rm -f cc1541-$(VERSION).tar

dist-check: dist
	tar xf cc1541-$(VERSION).tar.gz
	$(MAKE) -C cc1541-$(VERSION)/ check
	rm -rf cc1541-$(VERSION)/

codestyle: cc1541.c test_cc1541.c
	astyle --style=kr -n -s -z2 cc1541.c test_cc1541.c

wrap: README.md
	fold -s -w 70 < README.md | \
	  perl -pe 's/[\t\040]+$$//' > README.md.T
	mv -f README.md.T README.md

clean:
	rm -rf cc1541-$(VERSION)/ cc1541-$(VERSION).tar* README.md.T *.o *.orig cc1541 test_cc1541 *~

.PHONY: all check clean codestyle dist dist-check dist-bz2 dist-gz dist-xz install test wrap
