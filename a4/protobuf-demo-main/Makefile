CC = gcc
CFLAGS = -Wall -Werror -ggdb

# we're linking with an external library (protobuf-c)
LDLIBS = -lprotobuf-c

# because we're `#include`-ing the .pb-c.h files in ./src/protobuf, we need to provide the relevant -I flags
PROTOBUF_HEADER_DIRS := ./src/protobuf
INC_FLAGS := $(addprefix -I, $(PROTOBUF_HEADER_DIRS))

all: server client

# move to src/protobuf and run the protobuf compiler
src/protobuf/helloworld.pb-c.h src/protobuf/helloworld.pb-c.c: src/protobuf/helloworld.proto
	@echo "Generating protobuf C bindings"
	cd src/protobuf && \
		protoc --c_out=. helloworld.proto

# build server
server: src/protobuf/helloworld.pb-c.c src/server/server.c
	@echo "Building server"
	$(CC) $(CFLAGS) $(INC_FLAGS) -o server $^ $(LDLIBS)

# build client
client: src/protobuf/helloworld.pb-c.c src/client/client.c
	@echo "Building client"
	$(CC) $(CFLAGS) $(INC_FLAGS) -o client $^ $(LDLIBS)

clean:
	rm -f server client src/protobuf/*.pb*

.PHONY: clean all