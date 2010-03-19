CPPFLAGS := -I/usr/local/include/opencv -I./cvblobslib -Wall
CPP := g++

kenken: kenken.o
	$(CPP) $(CPPFLAGS) kenken.o -lm -lcv -lhighgui -lcxcore -L./cvblobslib -lblob -o $@
