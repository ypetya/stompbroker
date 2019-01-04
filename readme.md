STOMP Broker
============

This is not a safe or production ready implementation,
and still is in "Work in progress" phase.
TTL = 0

More information about STOMP protocol : https://stomp.github.io/stomp-specification-1.2.html
Implemented stomp scenarios can be found in functional tests: js/stomp.protocol.test.js

Websocket implementation is intended to follow RFC6455 with limitations:
It is still under implementation.
- lacks of security
- no fragmented messages
- will follow message size limitations globally configurable

Performance
-----------

current implementation uses epoll:
input queue is processed by a single thread

It makes some backpressure with starting as many output thread as free cores left.
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
~ Parse arguments (using pre-defined values, not accepting command line arguments)
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
~ subscription limit
+ connection limit (32000) by default
+ input queue limit
+ maximum message size
+ multithreaded output processing
- performace testing
+ LICENSE
+ move code one level up
+ publish to github
~ create tests and nodejs applications, see `js` directory
+ js/stomp.protocol.test.js
+ Websocket handshake filter
~ Websocket data frames encoding-decoding (Limited: text-only,no fragments)
- WS: Buffer underrun, Buffer overflow
- STOMP: Buffer overflow, multiple messages
- Grouped diagnostic message for session_stats
? Grouped diagnostic message for network io ( dropped ws data-frames, fixed underruns, cache size )
- Take maximum STOMP message size mandatory for WS as well
- WS ping-pong
- Implement TTL>0 message queue behaviour
```

Running
-------

/docker folder -> available docker files: for compiling and running

