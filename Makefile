cc1541: cc1541.o
	gcc -o $@ $^

clean:
	rm -rf *.o cc1541 *~
