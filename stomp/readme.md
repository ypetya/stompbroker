STOMP Broker
============

More information : https://stomp.github.io/stomp-specification-1.2.html

This folder is in the state of "Work in progress"

Performance
-----------

current implementation uses epoll:
input queue is processed by a single thread

On my laptop
for 20000 connections with 20000 '/queue/*' subscriptions
takes 2 seconds to deliver a single message on localhost ( using 4 nodejs processes as clients with console output)

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
~ Parse arguments
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
- ACK
- NACK
+ DISCONNECT
+ MESSAGE
+ RECEIPT
+ ERROR
+ using epoll instead of select
+ wildcard : only subscriptions allowed
~ subscription limit
+ input queue limit
- performace testing
```

Running
-------

/docker folder -> available docker files: for compiling and running

### STOMP protocol implementation

The goal is to implement features in v1.2 way.
Features implemented:

- connecting
