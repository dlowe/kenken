CFLAGS := -I/usr/local/include/opencv -std=c99 -Wall -pedantic
CC := gcc

.PHONY: test all clean

SOURCES := kenken.c c_blob.cpp
HEADERS := kenken.h c_blob.h
OBJECTS := kenken.o c_blob.o

all: test

test_locate_puzzle: $(OBJECTS) test_locate_puzzle.o
	gcc $(CFLAGS) $(OBJECTS) test_locate_puzzle.o -lm -lcv -lhighgui -lcxcore -lblob -lyaml -lstdc++ -o $@

c_blob.o: c_blob.cpp c_blob.h
	g++ -c -I/usr/local/include/opencv c_blob.cpp -o $@

test: test_locate_puzzle
	./test_locate_puzzle

clean:
	rm -f dependencies.mk
	rm -f test_locate_puzzle test_locate_puzzle.o
	rm -f $(OBJECTS)

-include dependencies.mk

dependencies.mk: test_locate_puzzle.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -MM $(SOURCES) test_locate_puzzle.c > dependencies.mk
