CFLAGS := -isystem /usr/local/include/opencv -std=c99 -Wall -pedantic -Werror
CC := gcc

.PHONY: test all clean

SOURCES := kenken.c
HEADERS := kenken.h
OBJECTS := kenken.o

all: test_locate_puzzle

test_locate_puzzle: $(OBJECTS) test_locate_puzzle.o
	$(CC) $(CFLAGS) $(OBJECTS) test_locate_puzzle.o -lm -lcv -lhighgui -lcxcore -lyaml -o $@

test: test_locate_puzzle
	./test_locate_puzzle --all

clean:
	rm -f dependencies.mk
	rm -f test_locate_puzzle test_locate_puzzle.o
	rm -f $(OBJECTS)

-include dependencies.mk

dependencies.mk: test_locate_puzzle.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -MM $(SOURCES) test_locate_puzzle.c > dependencies.mk
