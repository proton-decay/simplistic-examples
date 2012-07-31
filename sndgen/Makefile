CC=gcc
compile_options=-Wall -Wextra
alsa_libs=`pkg-config --libs alsa`
math_libs=-lm

sndgen: sndgen.c Makefile
	$(CC) -o sndgen sndgen.c $(alsa_libs) $(math_libs) $(compile_options)


