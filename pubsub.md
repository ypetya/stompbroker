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
match the remaining string.

### Worker queue

A worker queue message exchange delivers a message to exactly one consumer.
Every message gets delivered only once onto the next subscriber.
It is a round-robin-"ish" stlye of delivery. During the message processing new
consumers can join the subscription to share the load or disconnects can happen.

To configure a topic for this setup, the topic must be marked!

#### Limitations

- topic-name must be start with special character: `#` for example(`#example-exchange/topic/1`)
- the current implementation maintains only one global counter which has a modulo 
to the matching subscriptions. This spreads the delivery.
=> Using only one topic is guaranteed to have its order.

## Importance of TTL : Time to Live

At the current moment TTL is configurable globally.

### The TTL == 0 special case

In this case every message "dies" after picked up by the broker processing.
Without ACKnowledgments this ensures maximum throughput on a lossy manner.
No special internal queues gets used for pending messages.

### TTL > 0 case

A message sent to a destination without a matching subscription at that time
gets into a special queue (the `stale_queue`) instead of being dropped.

- The next `SUBSCRIBE` whose destination pattern matches (wildcards included)
  receives the stored messages at once, in the order they were sent, as normal
  `MESSAGE` frames: `destination` is the destination of the message,
  `subscription` the id of the new subscription. A stored message is delivered
  once: the first matching subscriber takes it.
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

To ensure message delivery is persistent STOMP defines a header named `ack` to
send with SUBSCRIBE frame. This can have 3 different values:

- `auto`: default, no ACK/NACK needed. message loss is possible
- `client`: client has to send ACK/NACK messages. all the sent messages by server
    gets to acknowledged state which has a smaller id then the reply and were
    sent to the specific client
- `client-individual`: client has to send ACK/NACK messages for all the messages.

In case NACK or time-out, the server tries to deliver a message to a different
consumer if it is available.

### Requirements (ACK)

- `ack_timeout` is a different constant that has to be defined in case TTL>0 !
- consider the internal waiting-for-acknowledgement queue size
