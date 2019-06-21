test_cc1541: test_cc1541.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -std=c99 $(CFLAGS) -c $<

test: test_cc1541
	./test_cc1541 ../cc1541

check: test

clean:
	rm -rf *.o test_cc1541 *~
