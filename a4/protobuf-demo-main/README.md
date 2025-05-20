# protobuf-demo

## DIRECTIONS
Run the following inside your container to install the protobuf compiler and produce the binaries. You can check out how to use `server` and `client` with the `--help` options. (`./server --help` and `./client --help`)
### C users
```sh
sudo apt update && sudo apt install protobuf-c-compiler libprotobuf-c-dev protobuf-compiler -y
```
Then run `make`.
### C++ users
```sh
sudo apt update && sudo apt install libprotobuf-dev protobuf-compiler -y
```
Then run `make -f Makefile-cpp`.

## What is ProtoBuf?

[ProtoBuf](https://protobuf.dev/programming-guides/proto2/) is Google's language-agnostic data serialization scheme.

You can define `message`s (structs/classes) via ProtoBuf definition, and the ProtoBuf compiler will generate bindings that can be used to manipulate the `message`s.

Neat thing about this is that any program could serialize and deserialize the defined messages, regardless of the language used, because ProtoBuf will handle the conversion between wire format and the language's native memory representation.

You can think of this as *ProtoBuf doing the `memcpy()`s for you*.

For this reason, it is used heavily in microservices architectures. While it is irrelevant for this course, you can take a look at **gRPC** if you want to learn more.

## Enterprise-Grade MSA-Oriented Hello world service

This demo is composed of two parts: `server` and `client`.

The `client` sends an Enum value and a repeat count to the server.

The `server` will then find the string matching with the requested enum value, and reply with the string repeated with the requested repeat count.

i.e. If `client` requests `0` (`Hello, world!`) with count `3`, the server will reply with `Hello, world!Hello, world!Hello, world!`.

## What you should check out before starting A4

See `helloworld.proto` for the ProtoBuf definitions our `client` and `server` is using.

Look at the [protobuf-c](https://protobuf-c.github.io/protobuf-c/pack.html) manual or [C++ tutorial](https://protobuf.dev/getting-started/cpptutorial/) to learn how to perform packing/unpacking. 

You could also take a look at the header the ProtoBuf compiler generates, as well as the client/server code to see how to use it.

See the `Makefile` and try to make sense of each part. Notice the `-I` flag and `-l` flag passed into the compiler.
