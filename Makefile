CC = gcc
CFLAGS = -W -Wall
LDFLAGS = -ldl
LIBFLAGS = -shared -Wl,-soname,$@

all: dumpmem

dumpmem.o: dumpmem.c
	$(CC) $(CFLAGS) -c -o $@ $<
dumpmem: dumpmem.o
	$(CC) $(LDFLAGS) -o $@ $<
.PHONY: clean
clean:
	rm -f *.o

.PHONY: distclean
distclean: clean
	rm -f dumpmem
