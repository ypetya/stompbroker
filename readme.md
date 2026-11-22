STOMP Broker
============

This is not a safe or production ready implementation, and still is in "Work in 
progress" phase. This is an experiment with messaging patterns.
Tests are written mainly in javascript, which you can find under the folder
examples/js.

Used protocols
---------------

*Stomp* implementation is tend to follow the protocol description at 
https://stomp.github.io/stomp-specification-1.2.html

*Websocket* implementation is intended to follow RFC6455 with some limitations:

Limitations
-----------

Some features are under implementation, especially in the following area

- Lack of security (WS/STOMP): no TLS, no authentication
- Taking only limited size messages: (WS/STOMP). See config on startup.
  A WebSocket message (single frame or reassembled fragments) may be at most
  1 000 000 bytes (`WS_DATA_FRAME_MAX_LENGTH`), larger ones are closed with 1009.
- No WebSocket extensions (e.g. permessage-deflate); UTF-8 of text frames is not validated.

WebSocket
---------

- Handshake (RFC 6455 4.2): request is buffered until the empty line (max 8 KiB),
  headers are case-insensitive. Requires `Host`, `Upgrade: websocket`,
  `Connection: Upgrade`, a valid `Sec-WebSocket-Key` and
  `Sec-WebSocket-Version: 13`. Errors: `400` malformed, `426` wrong version,
  `403` rejected by an allowlist.
- Subprotocol: `Sec-WebSocket-Protocol` is echoed when the client offers
  `v12.stomp`, `v11.stomp` or `v10.stomp` (the highest offered version wins).
- Origin / Host checks are opt-in (default: allow all), see
  `ws_allowed_origins` / `ws_allowed_hosts` below. Requests without an
  `Origin` header (non-browser clients) are not subject to the Origin list.
- Frames: any frame size, fragmentation (continuation frames, control frames
  between fragments), ping is answered with pong, close is echoed and the TCP
  connection is closed afterwards. Unmasked client frames, reserved bits,
  unknown opcodes or invalid control frames close the connection with 1002.
- **Binary frames policy:** binary data frames are accepted and treated exactly
  like text frames, i.e. the payload is a STOMP frame (a body may contain NUL
  when the frame has a `content-length` header). The broker always answers with
  text frames.
- Transport detection: a new connection whose first bytes are `GET ` is an
  HTTP upgrade, anything else is raw STOMP over TCP. Once decided, the
  connection never switches.
- Messaging patterns

STOMP
-----

STOMP 1.2 over raw TCP and over WebSocket (the same frame handling for both).

Frames from clients:

| Frame | Headers used | Notes |
|---|---|---|
| `CONNECT` / `STOMP` | `accept-version` | Version negotiation: the highest of `1.2`, `1.1`, `1.0` the client offers; no `accept-version` means a 1.0 client. `CONNECTED` carries `version`, `session`, `server`, `heart-beat:0,0`. No common version: `ERROR` with `version:1.0,1.1,1.2`. |
| `SEND` | `destination` (required, no `*`), `content-length`, `content-type`, any custom header | Delivered as `MESSAGE` to every matching subscription (worker queue topics starting with `#`: to one, see [Messaging](pubsub.md)). |
| `SUBSCRIBE` | `id`, `destination` (both required), `ack` (`auto`, `client`, `client-individual`), `prefetch-count` (default 1) | `destination` may end in `*` (prefix match). An `id` already used by the connection is an `ERROR`. Stored TTL messages that match are delivered at once (worker queues with `ack` other than `auto`: `prefetch-count` of them). |
| `UNSUBSCRIBE` | `id` (required) | Removes the connection's subscription with that id; its unacknowledged worker queue messages go to other workers. With TTL 0, stored worker queue jobs that no remaining subscription matches are dropped (also when a session ends). |
| `ACK` / `NACK` | `id` (required): the `ack` header of the `MESSAGE` | Worker queues only: settles the message (`client`: and the earlier ones of the subscription). A NACKed message goes to another worker. Unknown ids are ignored. |
| `DISCONNECT` | `receipt` | Subscriptions are removed, `RECEIPT` is sent when requested, then the broker closes the connection. |
| `DIAG` | body: query | Non standard diagnostics, see `examples/js/websocket/stomp.diag.*.js`. |

Every client frame may carry `receipt`: the broker answers with
`RECEIPT\nreceipt-id:<value>\n\n` + NUL after the frame is processed.

Frames from the broker:

- `MESSAGE`: the `SEND` headers (without `receipt`) plus `destination`,
  `subscription` (the subscription id), `message-id`, `content-length` and
  `content-type` (default `text/plain`). Worker queue messages to a
  subscription with `ack:client`/`ack:client-individual` carry `ack`;
  redelivered ones `redelivered:true`.
- `ERROR`: `message` (short reason), `receipt-id` (when the offending frame
  had `receipt`), `content-type`, `content-length`, the reason as body. After an
  `ERROR` the broker ends the session (no more deliveries) and closes the
  connection.

Framing:

- A frame ends at its NUL; with a `content-length` header it ends after that
  many body bytes, which must be followed by NUL (the body may contain NUL).
  A `content-length` that does not match is an `ERROR`.
- Frames may span TCP reads and several frames may arrive in one read.
  Frames larger than `max_frame_size` (default 1 000 000 bytes) get an `ERROR`
  and the connection is closed.
- EOLs between frames (heart-beats) are ignored; heart-beating is not
  negotiated (`heart-beat:0,0`).
- Commands and header names are case-sensitive and matched exactly; of
  repeated headers the first one counts. Header lines may end with CRLF.
- Malformed frames (no EOL after the command, a header line without `:`,
  headers not ended by an empty line) get an `ERROR`.

Not supported: `ACK`/`NACK` on pub-sub destinations (accepted, nothing is
tracked) and STOMP 1.1 `ACK` by `message-id`, transactions
(`BEGIN`/`COMMIT`/`ABORT`), header value escaping (`\n`, `\c`, `\\` are passed through as they are),
authentication.

Messaging
---------

See [Messaging](pubsub.md)

Configuration
-------------

### Common mistakes

Number of connections are limited:
Too many open file error can occure, oncrease the file limits for 
chlid-processes with 

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
max_stale_queue_size=<num>  : stale queue limit (number of messages)
max_stale_queue_bytes=<num> : stale queue memory limit in bytes (default 64000000)
max_frame_size=<num>        : largest STOMP frame in bytes (default 1000000), larger ones get ERROR + close
TTL=<num>                   : Time to live limit in <milli seconds> (0: messages without subscribers are dropped)
ack_timeout=<num>           : worker queue messages not acknowledged within <milli seconds> are redelivered (default 0: never)
write_timeout=<num>         : drop a slow consumer after <milli seconds> of waiting for it in total (default 5000); waiting restarts once it has taken one output buffer (10000 bytes)
ws_allowed_origins=<a,b>    : WebSocket Origin allowlist, comma separated (default: any)
ws_allowed_hosts=<a,b>      : WebSocket Host allowlist, comma separated, with or without port (default: any)
```

Build and test
--------------

```
make build          # Linux only (epoll)
make test           # unit tests: test.out + STOMP tests (parser, framing, handlers, close path, worker queues, ACK/NACK) under ASan/UBSan
make test-ws        # WebSocket unit tests under ASan/UBSan (Linux and macOS)
make test-stomp     # STOMP unit tests only (Linux and macOS)
make test-tsan      # STOMP unit tests under ThreadSanitizer
make test-e2e       # end-to-end: WebSocket, raw TCP STOMP (e2e/stomp_tcp_e2e.py) and, when node is
                    # installed, examples/js/raw/stomp.protocol.test.js; brokers start on free ports (Linux)
make test-e2e-asan  # raw TCP end-to-end test against an ASan/UBSan build of the broker (Linux)
make docker-test    # all of the above (but test-tsan) in a Linux container, uses podman or docker
make docker-test-tsan
```

The broker itself needs Linux (epoll); on macOS `build.sh` stops with a hint.
The unit tests are portable; use the container targets for the rest.

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
~ Parse arguments (Accepting: processors,port,max_input_queue_size, max_stale_queue_size,TTL)
+ Logger: Log output if necessary
+ Segregate main modules
+ TCP connection parent listener
+ TCP connection listener (frames may span reads, see max_frame_size)
+ Socket reader
+ STOMP frame parser
+ Implementing command execution / broker
+ Socket writer
+ SEND
+ SUBSCRIBE (handle id header for subscription!)
+ UNSUBSCRIBE
+ ACK (worker queues: ack:client, ack:client-individual, prefetch-count)
+ NACK (redelivery to another worker)
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
+ examples/js/raw/stomp.protocol.test.js  - RAW frame tests, run by `make test-e2e`
+ STOMP unit tests (test_stomp.c) and raw TCP end-to-end tests (e2e/stomp_tcp_e2e.py)
+ examples/js/websocket/* performance, TTL, diagnostic messages
- Create one functional test in javascript that tests all the test-cases. STOMP protocol, connection, load scenarios.
+ Websocket handshake filter
+ Websocket data frames encoding-decoding (all opcodes, fragmentation, any frame size)
+ WS: Buffer underrun, Buffer overflow
+ Make session threadsafe : use it only upfront!
+ STOMP: frame reassembly, multiple frames per read, content-length, max_frame_size
~ performace testing
+ Diagnostic message for session_stats `examples/js/stomp.diag.session.js`
+ Diagnostic message for ws_buff `examples/js/stomp.diag.ws.js`
+ Diagnostic message for checking internal queue sizes `examples/js/stomp.diag.stale.js`
? Internal benchmarks for possible bottle-necks + Diagnostic message
? ( dropped ws data-frames, fixed underruns, cache size )
+ WS frame maximum: WS_DATA_FRAME_MAX_LENGTH
+ WS buffering stats DIAG messages
+ WS ping-pong, close handshake
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
+ message exchange "Workerqueue distribution" with special topic name '#': round-robin per destination (skewed by wildcard workers, see pubsub.md), backlog split by prefetch-count
+ Introduce new internal queue for ACK/NACK: pending acknowledgements, redelivery on NACK, session end and command line arg ack_timeout
? multiple message dispatcher threads
+ Lower CPU load: replace sleeping thread loops with mutex to wait/notify awake in threadsafe queue
+ balanced tree implementation for associative array
```

Running
-------

A) You can build it with `./build.sh` having gcc and build essentials installed 
on a 64bit arhitecture.

B) Docker files are located under the folder `/docker`

C) There is an npm module distribution available for nodejs running on linux


```
npm i -g stompica
echo "require('stompica').runDefault()" | node
```

or you can pass arguments from nodejs:

```
require('stompica').run(0,5555,0,0,5000)
```

arguments have to be numbers in the following order:
processors, port, input_q, stale_q, TTL millies

value 0 -> configure to the default
