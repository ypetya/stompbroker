STOMP Broker
============

This is not a safe or production ready implementation,
and still is in "Work in progress" phase.

More information about STOMP protocol : https://stomp.github.io/stomp-specification-1.2.html
Implemented stomp scenarios can be found in functional tests: js/stomp.protocol.test.js

Websocket implementation is intended to follow RFC6455 with limitations:
It is still under implementation.
- lacks of security
- no fragmented messages yet
- taking only limited size messages: (see config on startup)

Configuration
-------------

### Common mistakes

Too many open file errors: Increase the file limits for process with 

```
ulimit -n 50000
```

### via command line args

Every argument are optional
```
Usage example:

./stompbroker.out processors=10 port=3000 TTL=5000 

processors=<num>            : writers count is processors-2 or at least 1
port=<num>                  : port to listen to
max_input_queue_size=<num>  : input queue limit
TTL=<num>                   : Time to live limit in <mikro seconds>
```

Performance
-----------

current implementation uses epoll:
input queue is processed by a single thread

It makes backpressure with starting as many output thread as free cores left.
In the future it must be balanced.
Further measurements needed.

Feature Status
--------------

The following list are notes about implementing features in priority order

The first character of the line can contain the following status codes:

```
?: To be defined, unknown or need decomposition
+: Implemented and ready
-: Not implemented yet
~: Implemented, but it is incomplete
```


```
~ Parse arguments (Accepting: processors,port,max_input_queue_size,TTL)
+ Logger: Log output if necessary
+ Segregate main modules
+ TCP connection parent listener
~ TCP connection listener (it can not read larger messages than buffer)
+ Socket reader
+ STOMP frame parser
+ Implementing command execution / broker
+ Socket writer
+ SEND
+ SUBSCRIBE (handle id header for subscription!)
+ UNSUBSCRIBE
- BEGIN
- COMMIT
- ABORT
? ACK (missing, no persistance)
- NACK
+ DISCONNECT
+ MESSAGE
+ SEND
+ MESSAGE custom headers
+ MESSAGE subscription
+ RECEIPT
+ ERROR
+ using epoll instead of select
+ wildcard : only subscriptions allowed
- subscription limit
+ connection limit
+ input queue limit
+ maximum message size
+ multithreaded output processing
~ performace testing
+ LICENSE
+ move code one level up
+ publish to github
~ create tests and nodejs applications, see `js` directory
+ js/stomp.protocol.test.js
+ Websocket handshake filter
~ Websocket data frames encoding-decoding (Limited: opcode=1,8 text-content,fin=1 no fragments)
+ WS: Buffer underrun, Buffer overflow
+ Make session threadsafe : use it only upfront!
- STOMP: Buffer overflow, multiple messages
- Grouped diagnostic message for session_stats
? Grouped diagnostic message for network io ( dropped ws data-frames, fixed underruns, cache size )
+ WS frame maximum: WS_DATA_FRAME_MAX_LENGTH
+ WS buffering stats DIAG messages
- WS ping-pong
+ peek messages: pick multiple messages for same FD (output queue only)
+ output buffering: every writer thread has an own 10k buffer for sending out multiple messages in a batch
+ handle WS client disconnect (opcode: 8)
+ more statistics, internal benchmark DIAGnostic messages
+ TTL: minimal impl: config, put message back if < TTL in reader thread
- persistance: save messages to file if defined (high io need, needs benchmark stats )
- replayability: start picking up and replay messages after a defined delay from file
- change processors params name: 1) to reflect writer threads count 2) be more intuitive
- add basic help and upload binary
- Create one functional test in javascript that tests all the test-cases. STOMP protocol, connection, load scenarios.
- Investigate POSIX message queues: man mq_overview
```

Running
-------

/docker folder -> available docker files: for compiling and running

