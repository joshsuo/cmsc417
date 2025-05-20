# Assignment 4

## DUE
**April 25, 2024 11:59:59 PM ET**

## Provided Resources
See the assignment specification on ELMS.

See the [example hash code here](https://gitlab.cs.umd.edu/cmsc417-s24/all/materials/-/tree/main/hash).

See the [protobuf demo here](https://gitlab.cs.umd.edu/cmsc417-s24/all/protobuf-demo).

## Chord Client
Args:
* -p \<Number> The port that the Chord client will bind to and listen on. Represented as a base-10 integer. Must be specified.
* --sp \<Number> The time in deciseconds between invocations of 'stabilize'. Represented as a base-10 integer. Must be specified, with a value in the range of [1, 600].
* --ffp \<Number> The time in deciseconds between invocations of 'fix_fingers'. Represented as a base-10 integer. Must be specified, with a value in the range of [1, 600].
* --cpp \<Number> The time in deciseconds between invocations of 'check_predececssor'. Represented as a base-10 integer. Must be specified, with a value in the range of [1, 600].
* --ja \<String> The IP address of the machine running a Chord node. The Chord client will join this node’s ring. Represented as an ASCII string (e.g., 128.8.126.63). Must be specified if --jp is specified.
* --jp \<String> The port that an existing Chord node is bound to and listening on. The Chord client will join this node’s ring. Represented as a base-10 integer. Must be specified if --ja is specified.
* -r \<Number> The number of successors maintained by the Chord client. Represented as a base-10 integer. Must be specified, with a value in the range of [1, 32]
* -i <String> = The identifier (ID) assigned to the Chord client which will override the ID computed by the truncated SHA1 sum of the client’s IP address and port number. Represented as a base-10 integer. Optional parameter.

An example usage to start a new Chord ring is:
```
chord -p 4170 --sp 5 --ffp 6 --cpp 7 -r 4
```

An example usage to join an existing Chord ring is:
```
chord -p 4171 --ja 128.8.130.86 --jp 4170 --sp 5 --ffp 6 --cpp 7 -r 4
```

NOTE: The join address is only exemplary, there will be no Chord node running on cerf.

The Chord client will bind a socket to the port specified by -p.
If neither --ja nor --jp is specified, then the Chord client starts a new ring by invoking
‘create’. The Chord client will initialize the successor list and finger table appropriately (i.e., all
will point to itself).
Otherwise, the Chord client joins an existing ring by connecting to the Chord client specified
by --ja and --jp and invoking ‘join’. The initial steps the Chord client takes when joining the
network are described in detail in Section IV.E.1 “Node Joins and Stabilization” of the Chord
paper.
Periodically, the Chord client will invoke various stabilization routines in order to handle
nodes joining and leaving the network. The Chord client will invoke ‘stabilize’, ‘fix fingers’, and
‘check predecessor’ every --sp, --ffp, and --cpp deciseconds, respectively.


### Commands
The Chord client will handle commands by reading from stdin and writing to
stdout. There are two command that the Chord client must support: ‘Lookup’ and ‘PrintState’.
User input must be prepended by `> ` and output must be prepended by `< `.

All nodes are printed in the form:
```
<Node ID> <Node IP> <Node Port>
```

‘Lookup’ takes as input an ASCII string (e.g., “Hello”). The Chord client takes this string,
hashes it to a key in the identifier space, and performs a search for the node that is the successor
to the key (i.e., the owner of the key). The Chord client then outputs that node’s identifier, IP
address, and port. Example:
```
> Lookup Hello
< Hello 17870176168342380699
< 3567812885933644873 128.8.126.63 2001
```
Where the command is of the form:
```
Lookup <String>
```

Such that the output of `Lookup` is of the form:
```
<Lookup String> <Lookup String ID/Key>
<Node ID> <Node IP> <Node Port>
```

‘PrintState’ requires no input. The Chord client outputs its local state information at the
current time, which consists of:
1. The Chord client’s own node information
2. The node information for all nodes in the successor list
3. The node information for all nodes in the finger table
where “node information” corresponds to the identifier, IP address, and port for a given node.

An example sequence of command inputs and outputs at a Chord client is shown below. There
are three participants in the ring (including the Chord client itself) and -r is set to 2. You may
assume the same formatting for the input, and must match the same formatting for your ouput.
“>” and “<” represent stdin and stdout respectively. The input commands will be terminated
by newlines (‘\n’).

Example:
```
> Lookup Hello
< Hello 17870176168342380699
< 3567812885933644873 128.8.126.63 2001

> Lookup World
< World 8124633097568820307
< 9013248200782045821 128.8.126.63 2003

> PrintState
< Self 3567812885933644873 128.8.126.63 2001
< Successor [1] 8045897832921729661 128.8.126.63 2002
< Successor [2] 9013248200782045821 128.8.126.63 2003
< Finger [1] 8045897832921729661 128.8.126.63 2002
< Finger [2] 8045897832921729661 128.8.126.63 2002
...
< Finger [63] 8045897832921729661 128.8.126.63 2002
< Finger [64] 8045897832921729661 128.8.126.63 2002
```

Where `Self`, `Successor` and `Finger` each put out their respective Chord Node,
as laid out in the format above.

## Implementation
We'll be using Google Protocol Buffers (protobuf) for handing messages passed between nodes.

### chord.proto
The following messages are provided:
- `NotifyRequest`
- `NotifyResponse`
- `FindSuccessorRequest`
- `FindSuccessorResponse`
- `GetPredecessorRequest`
- `GetPredecessorResponse`
- `CheckPredecessorRequest`
- `CheckPredecessorResponse`
- `GetSuccessorListRequest`
- `GetSuccessorListResponse`

The supplementary message `Node` is also supplied, meant to be a *sub*-message, that contains the necessary information about a Chord Node.

We've also created the message `ChordMessage` which will contain **one of** the request/response messages listed above at a time. You can demux on the `ChordMessage`'s `msg_case` field, which will tell you which message it contains. See the [oneof example](https://github.com/protobuf-c/protobuf-c/wiki/Examples#oneofs) in the protobuf-c wiki.

Every `ChordMessage` has an *optional* `query_id` associated with it. If used, it should be set **randomly** for queries and echoe'd back in the response.

Note that some fields are optional and only useful for certain choices (namely, iterative vs. recursive).

### struct Message
You'll see the `Message` struct defined within `chord.h`. This is the format messages should be sent to other Chord nodes. Essentially, you must prepend the length of the protobuf message so that the receiver knows the size of the `ChordMessage`.

### Keys / Identifiers
Keys, or identifiers (ID), are based off a SHA-1 hash of data. We'll be truncating its SHA-1 hash to be only 64 bits for ease of use.

The identifier for a node should be the 64-bit truncated SHA-1 sum of the node’s IP address and port number, but how the IP address and port number are input to the hash function is up to you (e.g., as a string, or byte array). 

Please use the `sha1sum_truncated_head` function from `hash.h` to grab the first 64 bits of the SHA-1 hash in the form of an integer.
See `printKey` in `chord.c` on how to properly print out a 64 bit integer.

## Piazza
Please tag any questions pertaining to this assignment with `as-4`.
