cc1541: cc1541.o
	gcc  -o $@ $^

%.o: %.c
	gcc -std=c99 -c $<

clean:
	rm -rf *.o cc1541 *~
