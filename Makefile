CC=gcc
CFLAGS=-ggdb

all: library	main

library:
	$(CC) $(CFLAGS) -c -fpic mem1.c -Werror
	$(CC) -shared -o libmem1.so mem1.o
	$(CC) $(CFLAGS) -c -fpic mem2.c -Werror
	$(CC) -shared -o libmem2.so mem2.o
	$(CC) $(CFLAGS) -c -fpic mem3.c -Werror
	$(CC) -shared -o libmem3.so mem3.o
	$(CC) -shared -o libcontest1.so mem3.o
	$(CC) -shared -o libcontest2.so mem3.o

main: library

clean:
	rm -rf libmem1.so
	rm -rf libcontest1.so
	rm -rf mem1.o
	rm -rf libmem2.so
	rm -rf libcontest2.so
	rm -rf mem2.o
	rm -rf libmem3.so
	rm -rf mem3.o
	rm -rf main
	rm -rf main2
	rm -rf main3
