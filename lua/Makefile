CC=gcc
CFLAGS=-Wall -Wextra -pedantic -llua
LDFLAGS=`pkg-config --libs lua`

lua: lua.o
lua.o: Makefile lua.c

clear:
	rm *.o lua
