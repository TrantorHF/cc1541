test_cc1541: test_cc1541.o
	$(CC)  -o $@ $^

%.o: %.c
	$(CC) -std=c99 -c $<

test: test_cc1541
	./test_cc1541 ../cc1541

clean:
	rm -rf *.o test_cc1541 *~
