STOMP Broker
============

This is not a safe or production ready implementation, and still is in "Work in progress" phase. This is an experiment with messaging patterns on my own.
Tests are written mainly in javascript, which you can find under the folder
examples/js.

Used protocolls
---------------

*Stomp* implementation is tend to follow the protocol description at https://stomp.github.io/stomp-specification-1.2.html

*Websocket* implementation is intended to follow RFC6455 with some limitations:

Limitations
-----------

Some features are under implementation, especially in the following area

- Lack of security (WS/STOMP)
- No fragmented messages (WS)
- Taking only limited size messages: (WS/STOMP). See config on startup.
- Messaging patterns

Messaging
---------

See [Messaging](pubsub.md)

Configuration
-------------

### Common mistakes

Number of connections are limited:
Too many open file error can occure, oncrease the file limits for chlid-processes with 

```
ulimit -n 5000
```

### Config parameters via command line args

Every argument are optional
```
Usage example:

./stompbroker.out processors=10 port=3000 TTL=5000 

processors=<num>            : writers count is processors-2 or at least 1
port=<num>                  : port to listen to
max_input_queue_size=<num>  : input queue limit
max_stale_queue_size=<num>  : stale queue limit
TTL=<num>                   : Time to live limit in <milli seconds>
```

Performance
-----------

current implementation uses epoll:
input queue is processed by a single thread

It makes backpressure with starting as many output thread as free cores left.
In the future it must be balanced.
Further measurements needed.

In a TTL=0 case, 1 nodejs connection, 117 bytes messages over WS with 75
subscribers it was able to deliver 1M messages under 9 seconds on a 2014
mac-book air (i5). That means 120Mb / sec over loopback interface.

Feature Status
--------------

The following list are notes about implementing features in priority order

The first character of the line can contain the following status codes:

```:text
?: To be defined, unknown or need decomposition
+: Implemented and ready
-: Not implemented yet
~: Implemented, but it is incomplete
```

```:text
~ Parse arguments (Accepting: processors,port,max_input_queue_size,max_stale_queue_size,TTL)
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
- ACK
- NACK
- BEGIN
- COMMIT
- ABORT
+ DISCONNECT
+ MESSAGE
+ SEND
+ MESSAGE custom headers
+ MESSAGE subscription
+ RECEIPT
+ ERROR
+ using epoll instead of select
+ wildcard : only subscriptions allowed
- subscription limit => wont fix. would be great to have an overall memory limit instead
- subscriptions list -> tree
+ connection limit
+ input queue limit
+ maximum message size
+ multithreaded output processing
+ LICENSE
+ move code one level up
+ publish to github
~ create tests and nodejs applications, see `examples/js` directory
+ examples/js/raw/stomp.protocol.test.js  - RAW frame tests compiled with DEBUG mode in main.c
+ examples/js/websocket/* performance, TTL, diagnostic messages
- Create one functional test in javascript that tests all the test-cases. STOMP protocol, connection, load scenarios.
+ Websocket handshake filter
~ Websocket data frames encoding-decoding (Limited: opcode=1,8 text-content,fin=1 no fragments)
+ WS: Buffer underrun, Buffer overflow
+ Make session threadsafe : use it only upfront!
- STOMP: Buffer overflow, multiple messages
~ performace testing
+ Diagnostic message for session_stats `examples/js/stomp.diag.session.js`
+ Diagnostic message for ws_buff `examples/js/stomp.diag.ws.js`
+ Diagnostic message for checking internal queue sizes `examples/js/stomp.diag.stale.js`
? Internal benchmarks for possible bottle-necks + Diagnostic message
? ( dropped ws data-frames, fixed underruns, cache size )
+ WS frame maximum: WS_DATA_FRAME_MAX_LENGTH
+ WS buffering stats DIAG messages
- WS ping-pong
- STOMP heartbeat
+ peek messages: pick multiple messages for same FD (output queue only)
+ output buffering: every writer thread has an own 10k buffer for sending out multiple messages in a batch
+ handle WS client disconnect (opcode: 8)
+ more statistics, internal benchmark DIAGnostic messages
+ TTL impl: with stale_queue. subscribers check stale_queue for messages when ttl>0
- persistance: save messages to file if defined (high io need, needs benchmark stats )
- replayability: start picking up and replay messages after a defined delay from file
- change processors params name: 1) to reflect writer threads count 2) be more intuitive
+ add basic help and upload binary
+ message exchange PUB-SUB distribution by default
- message exchange Workerqueue distribution with special topic names
- Introduce new internal queue for ACK/NACK: ack_queue, with command line arg ack_timeout
? multiple message dispatcher threads
```

Running
-------

A) You can build it with `./build.sh` having gcc and build essentials installed on a 64bit arhitecture.

B) Docker files are located under the folder `/docker`
