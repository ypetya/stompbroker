STOMP Broker
============

More information : https://stomp.github.io/stomp-specification-1.2.html

This folder is in the state of "Work in progress"

Performance
-----------

current implementation uses epoll:
input queue is processed by a single thread
and as many output thread as free cores

Further measurements needed

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
+ SUBSCRIBE
+ UNSUBSCRIBE
- BEGIN
- COMMIT
- ABORT
? ACK (missing, no persistance)
- NACK
+ DISCONNECT
+ MESSAGE
+ SEND -> MESSAGE custom headers
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
```

Running
-------

/docker folder -> available docker files: for compiling and running

