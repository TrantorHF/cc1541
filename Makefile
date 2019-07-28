all: cc1541

cc1541: cc1541.o
	$(CC) -o $@ $^ $(LDFLAGS)

test_cc1541: test_cc1541.o
	$(CC) -o $@ $^ $(LDFLAGS)

check: cc1541 test_cc1541
	./test_cc1541 ./cc1541

test: check

clean:
	rm -rf *.o cc1541 test_cc1541 *~

%.o: %.c
	$(CC) -std=c99 $(CFLAGS) -c $<
