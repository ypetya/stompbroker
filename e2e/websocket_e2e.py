#!/usr/bin/env python3
"""
End-to-end test of the STOMP broker over WebSocket (RFC 6455) and raw TCP.

Standard library only. Starts the broker (when --broker is given), then runs
the scenarios below against it. Exits 0 when every scenario passes.

    python3 e2e/websocket_e2e.py --broker ./stompbroker.out --port 3499
    make test-e2e            # Linux (epoll); or inside `make docker-test`
"""
import argparse
import base64
import hashlib
import os
import socket
import struct
import subprocess
import sys
import time

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
TIMEOUT = 5.0

OP_CONT, OP_TEXT, OP_BINARY, OP_CLOSE, OP_PING, OP_PONG = 0x0, 0x1, 0x2, 0x8, 0x9, 0xA


class TestFailure(Exception):
    pass


def expect(condition, message):
    if not condition:
        raise TestFailure(message)


# --------------------------------------------------------------------------
# Clients
# --------------------------------------------------------------------------
class WSClient:
    """Minimal RFC 6455 client: masked frames out, unmasked frames in."""

    def __init__(self, port, protocols="v10.stomp, v11.stomp, v12.stomp", extra_headers="", version="13"):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=TIMEOUT)
        self.buf = b""
        self.key = base64.b64encode(os.urandom(16)).decode()
        request = (
            "GET /stomp HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {self.key}\r\n"
            f"Sec-WebSocket-Version: {version}\r\n"
            + (f"Sec-WebSocket-Protocol: {protocols}\r\n" if protocols else "")
            + extra_headers
            + "\r\n"
        )
        self.request = request.encode()

    def handshake(self, split=False):
        if split:  # request delivered in small TCP segments
            for i in range(0, len(self.request), 7):
                self.sock.sendall(self.request[i:i + 7])
                time.sleep(0.002)
        else:
            self.sock.sendall(self.request)
        while b"\r\n\r\n" not in self.buf:
            chunk = self.sock.recv(4096)
            expect(chunk, "connection closed during handshake")
            self.buf += chunk
        head, self.buf = self.buf.split(b"\r\n\r\n", 1)
        lines = head.decode().split("\r\n")
        headers = {}
        for line in lines[1:]:
            name, _, value = line.partition(":")
            headers[name.strip().lower()] = value.strip()
        return lines[0], headers

    def expected_accept(self):
        return base64.b64encode(hashlib.sha1((self.key + GUID).encode()).digest()).decode()

    def send_frame(self, opcode, payload=b"", fin=True, mask=True, rsv=0):
        if isinstance(payload, str):
            payload = payload.encode()
        header = bytes([(0x80 if fin else 0) | rsv | opcode])
        mask_bit = 0x80 if mask else 0
        n = len(payload)
        if n < 126:
            header += bytes([mask_bit | n])
        elif n < 65536:
            header += bytes([mask_bit | 126]) + struct.pack("!H", n)
        else:
            header += bytes([mask_bit | 127]) + struct.pack("!Q", n)
        if mask:
            key = os.urandom(4)
            payload = bytes(b ^ key[i % 4] for i, b in enumerate(payload))
            header += key
        return header + payload

    def send(self, opcode, payload=b"", fin=True, mask=True, rsv=0):
        self.sock.sendall(self.send_frame(opcode, payload, fin, mask, rsv))

    def send_stomp(self, frame):
        self.send(OP_TEXT, frame.encode() + b"\0")

    def _need(self, n):
        while len(self.buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise EOFError("connection closed")
            self.buf += chunk

    def recv_frame(self):
        self._need(2)
        b0, b1 = self.buf[0], self.buf[1]
        expect(b1 & 0x80 == 0, "server frames must not be masked")
        n, pos = b1 & 0x7F, 2
        if n == 126:
            self._need(4)
            n, pos = struct.unpack("!H", self.buf[2:4])[0], 4
        elif n == 127:
            self._need(10)
            n, pos = struct.unpack("!Q", self.buf[2:10])[0], 10
        self._need(pos + n)
        payload = self.buf[pos:pos + n]
        self.buf = self.buf[pos + n:]
        return b0 & 0x0F, bool(b0 & 0x80), payload

    def recv_stomp(self):
        """Next STOMP frame (text frame), skipping pongs."""
        while True:
            opcode, _, payload = self.recv_frame()
            if opcode == OP_PONG:
                continue
            expect(opcode == OP_TEXT, f"expected text frame, got opcode {opcode:#x}")
            return payload.rstrip(b"\0").decode()

    def expect_eof(self):
        try:
            self.sock.settimeout(TIMEOUT)
            while True:
                chunk = self.sock.recv(4096)
                if not chunk:
                    return
                self.buf += chunk
        except ConnectionResetError:
            return

    def close(self):
        self.sock.close()


class RawClient:
    """Plain STOMP over TCP (frames terminated by NUL)."""

    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=TIMEOUT)
        self.buf = b""

    def send_stomp(self, frame):
        self.sock.sendall(frame.encode() + b"\0")

    def recv_stomp(self):
        while b"\0" not in self.buf:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise EOFError("connection closed")
            self.buf += chunk
        frame, self.buf = self.buf.split(b"\0", 1)
        return frame.lstrip(b"\r\n").decode()

    def close(self):
        self.sock.close()


def stomp_connect(client):
    client.send_stomp("CONNECT\naccept-version:1.2\nhost:localhost\n\n")
    reply = client.recv_stomp()
    expect(reply.startswith("CONNECTED"), f"expected CONNECTED, got {reply[:40]!r}")


def stomp_subscribe(client, destination, sub_id, receipt):
    client.send_stomp(f"SUBSCRIBE\nid:{sub_id}\ndestination:{destination}\nreceipt:{receipt}\n\n")
    reply = client.recv_stomp()
    expect(reply.startswith("RECEIPT") and f"receipt-id:{receipt}" in reply, f"expected RECEIPT, got {reply[:60]!r}")


def message_body(frame):
    expect(frame.startswith("MESSAGE"), f"expected MESSAGE, got {frame[:60]!r}")
    return frame.split("\n\n", 1)[1]


def ws_session(port):
    ws = WSClient(port)
    status, headers = ws.handshake()
    expect(status.startswith("HTTP/1.1 101"), f"handshake failed: {status}")
    stomp_connect(ws)
    return ws


# --------------------------------------------------------------------------
# Scenarios
# --------------------------------------------------------------------------
def scenario_handshake(port):
    ws = WSClient(port)
    status, headers = ws.handshake(split=True)
    expect(status.startswith("HTTP/1.1 101"), f"status {status}")
    expect(headers.get("sec-websocket-accept") == ws.expected_accept(), "accept key mismatch")
    expect(headers.get("sec-websocket-protocol") == "v12.stomp", f"subprotocol {headers.get('sec-websocket-protocol')}")
    ws.close()


def scenario_bad_version(port):
    ws = WSClient(port, version="8")
    status, headers = ws.handshake()
    expect(status.startswith("HTTP/1.1 426"), f"status {status}")
    expect(headers.get("sec-websocket-version") == "13", "426 must advertise version 13")
    ws.expect_eof()
    ws.close()


def scenario_pubsub(port):
    sub = ws_session(port)
    stomp_subscribe(sub, "/topic/e2e", 1, "s1")
    pub = ws_session(port)
    pub.send_stomp("SEND\ndestination:/topic/e2e\n\nhello websocket")
    expect(message_body(sub.recv_stomp()) == "hello websocket", "wrong body")
    # a body that looks like an HTTP request must stay a STOMP body
    pub.send_stomp("SEND\ndestination:/topic/e2e\n\nGET / HTTP/1.1")
    expect(message_body(sub.recv_stomp()) == "GET / HTTP/1.1", "GET / body mangled")
    sub.close()
    pub.close()


def scenario_ping_pong(port):
    ws = ws_session(port)
    ws.send(OP_PING, b"are you there?")
    opcode, fin, payload = ws.recv_frame()
    expect(opcode == OP_PONG and fin and payload == b"are you there?", f"pong {opcode:#x} {payload!r}")
    ws.close()


def scenario_fragmented_send(port):
    ws = ws_session(port)
    stomp_subscribe(ws, "/topic/frag", 2, "s2")
    frame = b"SEND\ndestination:/topic/frag\n\nfragmented body\0"
    ws.send(OP_TEXT, frame[:10], fin=False)
    ws.send(OP_PING, b"mid")
    ws.send(OP_CONT, frame[10:25], fin=False)
    ws.send(OP_CONT, frame[25:], fin=True)
    opcode, _, payload = ws.recv_frame()
    expect(opcode == OP_PONG and payload == b"mid", "pong between fragments")
    expect(message_body(ws.recv_stomp()) == "fragmented body", "fragmented body")
    ws.close()


def scenario_byte_by_byte(port):
    ws = ws_session(port)
    stomp_subscribe(ws, "/topic/bytes", 3, "s3")
    data = ws.send_frame(OP_TEXT, b"SEND\ndestination:/topic/bytes\n\nx\0")
    for i in range(len(data)):
        ws.sock.sendall(data[i:i + 1])
        time.sleep(0.001)
    expect(message_body(ws.recv_stomp()) == "x", "byte-by-byte body")
    ws.close()


def scenario_large_message(port):
    ws = ws_session(port)
    stomp_subscribe(ws, "/topic/large", 4, "s4")
    body = "L" * 200000  # 64-bit length in, larger than the 10 KB writer buffer out
    ws.send_stomp("SEND\ndestination:/topic/large\n\n" + body)
    expect(message_body(ws.recv_stomp()) == body, "large body")
    ws.close()


def scenario_raw_stomp_unaffected(port):
    raw = RawClient(port)
    stomp_connect(raw)
    stomp_subscribe(raw, "/topic/mixed", 5, "s5")
    ws = ws_session(port)
    ws.send_stomp("SEND\ndestination:/topic/mixed\n\nfrom websocket")
    expect(message_body(raw.recv_stomp()) == "from websocket", "raw subscriber body")
    raw.send_stomp("SEND\ndestination:/topic/mixed\n\nGET / from raw")
    expect(message_body(raw.recv_stomp()) == "GET / from raw", "raw self-delivery")
    raw.close()
    ws.close()


def scenario_protocol_error_closes(port):
    ws = ws_session(port)
    ws.send(OP_TEXT, b"SEND\n\n\0", mask=False)  # clients must mask
    opcode, _, payload = ws.recv_frame()
    expect(opcode == OP_CLOSE and struct.unpack("!H", payload[:2])[0] == 1002, f"close {opcode:#x} {payload!r}")
    ws.expect_eof()
    ws.close()


def scenario_clean_close(port):
    ws = ws_session(port)
    ws.send(OP_CLOSE, struct.pack("!H", 1000) + b"bye")
    opcode, _, payload = ws.recv_frame()
    expect(opcode == OP_CLOSE and payload == struct.pack("!H", 1000), f"close echo {payload!r}")
    ws.expect_eof()  # broker closes TCP after the echo
    ws.close()


def scenario_pipelined_connect(port):
    """CONNECT sent in the same segment as the upgrade: 101 must come before CONNECTED."""
    for _ in range(50):
        ws = WSClient(port)
        connect = ws.send_frame(OP_TEXT, b"CONNECT\naccept-version:1.2\nhost:localhost\n\n\0")
        ws.sock.sendall(ws.request + connect)
        ws._need(len("HTTP/1.1 101"))
        expect(ws.buf.startswith(b"HTTP/1.1 101"), f"101 must be first, got {ws.buf[:20]!r}")
        while b"\r\n\r\n" not in ws.buf:
            ws._need(len(ws.buf) + 1)
        ws.buf = ws.buf.split(b"\r\n\r\n", 1)[1]
        reply = ws.recv_stomp()
        expect(reply.startswith("CONNECTED"), f"expected CONNECTED, got {reply[:40]!r}")
        ws.close()


SCENARIOS = [
    scenario_handshake,
    scenario_pipelined_connect,
    scenario_bad_version,
    scenario_pubsub,
    scenario_ping_pong,
    scenario_fragmented_send,
    scenario_byte_by_byte,
    scenario_large_message,
    scenario_raw_stomp_unaffected,
    scenario_protocol_error_closes,
    scenario_clean_close,
]


# --------------------------------------------------------------------------
# Runner
# --------------------------------------------------------------------------
def wait_for_port(port, deadline=10.0):
    end = time.time() + deadline
    while time.time() < end:
        try:
            socket.create_connection(("127.0.0.1", port), timeout=0.5).close()
            return
        except OSError:
            time.sleep(0.1)
    raise TestFailure(f"broker did not listen on {port}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=3499)
    parser.add_argument("--broker", help="broker binary to start (otherwise one must be running)")
    args = parser.parse_args()

    broker = None
    log = None
    if args.broker:
        log = open("e2e_broker.log", "w")
        broker = subprocess.Popen([args.broker, f"port={args.port}", "processors=4"], stdout=log, stderr=subprocess.STDOUT)
    failures = 0
    try:
        wait_for_port(args.port)
        for scenario in SCENARIOS:
            try:
                scenario(args.port)
                print(f"PASS {scenario.__name__}")
            except Exception as error:  # noqa: BLE001 - report every scenario
                failures += 1
                print(f"FAIL {scenario.__name__}: {type(error).__name__}: {error}")
        if broker is not None:
            expect(broker.poll() is None, "broker exited during the test")
    except TestFailure as error:
        failures += 1
        print(f"FAIL setup: {error}")
    finally:
        if broker is not None:
            broker.terminate()
            try:
                broker.wait(timeout=5)
            except subprocess.TimeoutExpired:
                broker.kill()
            log.close()

    print(f"e2e: {len(SCENARIOS)} scenarios, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
