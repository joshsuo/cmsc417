CC=gcc
CFLAG_INCLUDE=-I. -Iinclude -Iprotobuf
CFLAGS=-Wall $(CFLAG_INCLUDE) -Wextra -std=gnu99 -ggdb -fsanitize=address -fsanitize=undefined -lcurl
LDLIBS=-lprotobuf-c -lcrypto -lm
SRC_DIR = src
INCLUDE_DIR = include
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(SRC_FILES:$(SRC_DIR)/%.c=%.o)
EXECUTABLE = bitTorrent

.PHONY: all clean

all: bitTorrent

bitTorrent: bitTorrent.o bencode.o hash.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC_DIR)/bencode.c $(SRC_DIR)/hash.c $(SRC_DIR)/bitTorrent.c $(SRC_DIR)/util.c -o bitTorrent $(LDLIBS)

%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_FILES) $(EXECUTABLE)
