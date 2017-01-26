cc1541: cc1541.o XGetopt.o
	g++ -o $@ $^

clean:
	rm -rf *.o cc1541 *~
