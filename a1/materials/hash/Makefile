CC=gcc
CFLAGS=-Wall -Iincludes -Wextra -std=c99 -ggdb
LDLIBS=-lcrypto
VPATH=src

all: test

test: hash.o test.o

clean:
	rm -rf *~ *.o test


.PHONY : clean all
