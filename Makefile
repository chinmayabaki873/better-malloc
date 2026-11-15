CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC

all: libmymalloc.so test_main

libmymalloc.so: mymalloc.c mymalloc.h
	$(CC) $(CFLAGS) -shared -o libmymalloc.so mymalloc.c

test_main: test_main.c libmymalloc.so
	$(CC) -Wall -Wextra -O2 -o test_main test_main.c -L. -lmymalloc -Wl,-rpath,.

clean:
	rm -f *.o *.so test_main