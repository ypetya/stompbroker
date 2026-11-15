Some tests based on nodejs and stompjs overWS
=============================================

These scripts are manual load / diagnostic tools and need a running broker.

The automated WebSocket regression test is `e2e/websocket_e2e.py` (Python,
standard library only): handshake incl. subprotocol, pub/sub, ping/pong,
fragmented SEND, byte-by-byte frames, a 200 KB message, raw STOMP clients,
protocol error close and clean close. Run it with `make test-e2e` on Linux or
`make docker-test` anywhere podman/docker is available.

The broker supports fragmented WebSocket messages, ping/pong and the close
handshake, and echoes the `v10.stomp`/`v11.stomp`/`v12.stomp` subprotocol.
