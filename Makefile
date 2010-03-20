CPPFLAGS := -I/usr/local/include/opencv -I./cvblobslib
CPP := g++

solve: kenken.o solve.o
	$(CPP) $(CPPFLAGS) kenken.o solve.o -lm -lcv -lhighgui -lcxcore -L./cvblobslib -lblob -o $@

test_locate_puzzle: test_locate_puzzle.o kenken.o
	$(CPP) $(CPPFLAGS) test_locate_puzzle.o kenken.o -lm -lcv -lhighgui -lcxcore -L./cvblobslib -lblob -lyaml -o $@

test: test_locate_puzzle
	./test_locate_puzzle
