CPPFLAGS := -I/usr/local/include/opencv
CPP := g++

.PHONY: test

solve: kenken.o solve.o
	$(CPP) $(CPPFLAGS) kenken.o solve.o -lm -lcv -lhighgui -lcxcore -lblob -o $@

test_locate_puzzle: test_locate_puzzle.o kenken.o
	$(CPP) $(CPPFLAGS) test_locate_puzzle.o kenken.o -lm -lcv -lhighgui -lcxcore -lblob -lyaml -o $@

test: test_locate_puzzle
	./test_locate_puzzle

clean:
	rm -f test_locate_puzzle
	rm -f solve
	rm -f kenken.o solve.o test_locate_puzzle.o
