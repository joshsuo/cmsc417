CC=gcc
CFLAGS=-Wall -Iincludes -Wextra -ggdb
#CFLAGS=-Wall -Iincludes -Wextra -std=gnu99 -ggdb
VPATH=src

all: client server

client: client.c

server: server.c

clean:
	rm -rf client server *.o


.PHONY : clean all