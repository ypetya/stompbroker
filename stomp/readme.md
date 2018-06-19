STOMP Broker
============

More information : https://stomp.github.io/stomp-specification-1.2.html

This folder is in the state of "Work in progress"

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
~ Logger: Log output if necessary
~ Segregate main modules
~ TCP connection parent listener
- TCP connection listener
- Socket reader
- STOMP frame parser
- Implementing command execution / broker
- Socket writer
```