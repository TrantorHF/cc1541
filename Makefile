cc1541: cc1541.o
	$(CC)  -o $@ $^

%.o: %.c
	$(CC) -std=c99 -c $<

clean:
	rm -rf *.o cc1541 *~
