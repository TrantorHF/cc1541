bindir ?= /usr/local/bin

INSTALL ?= install


all: cc1541

cc1541: cc1541.o
	$(CC) -o $@ $^ $(LDFLAGS)

test_cc1541: test_cc1541.o
	$(CC) -o $@ $^ $(LDFLAGS)

check: cc1541 test_cc1541
	./test_cc1541 ./cc1541

test: check

install: all
	$(INSTALL) -Dpm 0755 ./cc1541 $(DESTDIR)$(bindir)/cc1541

clean:
	rm -rf *.o cc1541 test_cc1541 *~

%.o: %.c
	$(CC) -std=c99 $(CFLAGS) -c $<

.PHONY: all check clean install test
