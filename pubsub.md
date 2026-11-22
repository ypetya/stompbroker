# Messaging

This current document summarizes the messaging patterns covered by this project.

## Message exchange

The broker supports some basic messaging patterns for the following scanrios.

1) PUB-SUB: multiplexing messages from one source (the producer) to many targets
    (the consumers).
2) WORKER: load balancing message processing via worker queue, where one message
    can reach exactly one consumer.

### PUB-SUB

PUB-SUB message exchange delivers a message to all the subscriptions.
This is the default behaviour. When a message is sent to a specific topic, a
fan-out happens which delivers the message to all of the subscriptions which
have a matching topic pattern. A topic pattern can use wildcard caracter '*' to
match the remaining string, which may be empty: `/topic/*` matches `/topic/a`
and `/topic/` as well. Stored TTL messages are matched by the same rule.

### Worker queue

A worker queue message exchange delivers a message to exactly one consumer, so
a backlog of jobs is split across workers and each job is processed once.

To configure a topic for this setup, the destination must start with `#`, for
example `#example-exchange/jobs`. Every subscription whose pattern matches the
destination is a worker of it (`#example-exchange/*` too).

#### Distribution

- Round-robin: a message goes to the matching subscription that was chosen
  least recently (ties: the one subscribed first). Every subscription
  remembers one last pick. Workers that subscribe to a single destination
  rotate per destination: traffic to other `#` destinations does not skew it.
  A wildcard worker (for example `#*`) shares its last pick across every
  destination it matches, which skews the rotation (see Limitations). A
  worker that joins takes the next message; one that leaves is skipped.
- With `ack:auto` (the default) a message is done once it is handed to the
  output queue: at-most-once, no flow control. A worker that subscribes to a
  destination with stored messages (TTL > 0) takes all of them.
- With `ack:client` or `ack:client-individual` (see below) a worker gets at
  most `prefetch-count` (SUBSCRIBE header, default 1) messages it has not
  acknowledged yet. That is what splits a backlog: every worker takes one job
  (or `prefetch-count` jobs), the next one after its `ACK`.

#### Busy workers

When every matching worker has `prefetch-count` unacknowledged messages, a new
message waits in the stale queue (also when TTL == 0, the limits of the stale
queue apply) and goes to the first worker that acknowledges. With TTL > 0 a
waiting message still expires TTL after it was received. A message to a
`#` destination without any matching subscription is handled as any other
message: stored when TTL > 0, dropped when TTL == 0.

With TTL == 0 nothing expires waiting jobs, so they are dropped when no
subscription matches them any more: after an `UNSUBSCRIBE` or the end of a
session (`DISCONNECT`, `ERROR`, the connection drops), the stored `#` jobs
that no remaining subscription matches are dropped, including the
unacknowledged jobs the leaving worker returned. With TTL > 0 they stay until
they expire, for the next worker.

A full stale queue drops every expired message before it refuses a new one,
wherever it is stored (redelivered jobs are put before older messages).

#### Limitations

- A wildcard worker skews the round-robin. Its single last pick moves
  whenever it gets a message for any destination it matches. Example:
  workers `A` (`#a`) and `W` (`#*`), traffic alternating between `#a` and
  `#b`: every `#b` job goes to `W` (the only match) and makes it the most
  recently picked, so `A` gets more `#a` jobs than `W`. Balance is only
  guaranteed among workers that subscribe to the same single destination.

## Importance of TTL : Time to Live

At the current moment TTL is configurable globally.

### The TTL == 0 special case

In this case every message "dies" after picked up by the broker processing.
Without ACKnowledgments this ensures maximum throughput on a lossy manner.
No special internal queues gets used for pending messages, except for worker
queues with acknowledgements: their messages wait while every worker is busy
or until they are acknowledged (see Worker queue and ACK below).

### TTL > 0 case

A message sent to a destination without a matching subscription at that time
gets into a special queue (the `stale_queue`) instead of being dropped.

- The next `SUBSCRIBE` whose destination pattern matches (wildcards included)
  receives the stored messages at once, in the order they were sent, as normal
  `MESSAGE` frames: `destination` is the destination of the message,
  `subscription` the id of the new subscription. A stored message is delivered
  once: the first matching subscriber takes it. A worker queue subscription
  with `ack:client`/`ack:client-individual` takes only `prefetch-count` of the
  stored `#` messages; the rest wait for the other workers and its `ACK`s.
- A message expires TTL milliseconds after the broker received it (monotonic
  clock). Expired messages are dropped when a new message is stored or a
  subscription looks for messages; they are never delivered.
- Messages that wait in the input queue for longer than TTL are dropped too.

#### Requirements (TTL>0, stale_queue)

The stale queue is bounded, otherwise a flood of messages to destinations
without subscribers would exhaust memory:

- `max_stale_queue_size`: number of messages (default 1000000),
- `max_stale_queue_bytes`: memory, counted as body + destination + per message
  bookkeeping (default 64000000).

When a limit is reached, new messages without subscribers are dropped (a
warning is logged); expired messages are removed first.

## Acknowledgement: ACK, NACK frames

The `ack` header of `SUBSCRIBE` selects how messages are acknowledged:

- `auto`: default, no ACK/NACK needed. Message loss is possible.
- `client`: an `ACK` or `NACK` settles the message and every earlier message
  delivered to the same subscription (cumulative).
- `client-individual`: an `ACK` or `NACK` settles only that message.

Any other value is an `ERROR`; so is a `prefetch-count` that is not a whole
number >= 1.

Acknowledgements are tracked for worker queues (`#` destinations): their
`MESSAGE` frames to a subscription with `ack:client` or
`ack:client-individual` carry an `ack` header (none with `ack:auto`), and
`ACK`/`NACK` frames name it in
their `id` header (STOMP 1.2). Pub-sub deliveries stay at-most-once: they
carry no `ack` header and nothing is redelivered.

A worker queue message delivered to a subscription with `client` or
`client-individual` is kept until it is settled (at-least-once):

- `ACK`: the message is done; the worker gets the next waiting message.
- `NACK`: the message goes to another matching worker that has room; if there
  is none it waits before the newer messages (the NACKing worker may get it
  again).
- the worker's subscription or session ends (`UNSUBSCRIBE`, `DISCONNECT`,
  `ERROR`, the connection drops): its unacknowledged messages go to the other
  workers, or wait for the next one (also when TTL == 0).
- `ack_timeout` (command line, milli-seconds, default 0: off): a message not
  acknowledged within it is redelivered as after a `NACK`. Time-outs are
  checked at least every 100 ms.

A redelivered `MESSAGE` has the header `redelivered:true`; with TTL > 0 a
message older than TTL is dropped instead of redelivered. An `ACK`/`NACK` with
an unknown id (already settled, timed out, of another connection) is ignored,
because the message may already be with another worker. Without an `id`
header it is an `ERROR`.

### Limitations (ACK)

- STOMP 1.0/1.1 style `ACK` with `message-id` (instead of `id`) is not
  supported.
- No transactions (`BEGIN`/`COMMIT`/`ABORT`, `transaction` header).
- Unacknowledged messages are bounded by `prefetch-count` per subscription;
  redelivered messages may exceed the stale queue limits (they were accepted
  before).
- Every `ACK`/`NACK` scans the stale queue for a message the worker matches.
- A worker that stays connected but stops acknowledging is never
  disconnected: `ack_timeout` redelivers its messages to other workers, but it
  keeps its subscription (and receives new messages as its credit frees up).
- At-least-once: a slow worker that acknowledges after `ack_timeout` and the
  worker that got the redelivery may both process the same job.
- `ack_timeout` is converted to micro-seconds in a 32-bit value, so the
  largest working value is 4294967 ms (about 71 minutes); larger values wrap
  around silently to a much shorter time-out.
