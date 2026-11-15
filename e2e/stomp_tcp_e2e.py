#!/usr/bin/env python3
"""
End-to-end test of STOMP 1.2 over raw TCP.

Standard library only. Starts the broker on an ephemeral port (when --broker
is given), runs TCP probes against it and exits 0 when every scenario passes.

    python3 e2e/stomp_tcp_e2e.py --broker ./stompbroker.out
    make test-e2e            # Linux (epoll); or inside `make docker-test`
"""
import argparse
import socket
import subprocess
import sys
import time

TIMEOUT = 5.0


class TestFailure(Exception):
    pass


def expect(condition, message):
    if not condition:
        raise TestFailure(message)


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Client:
    """Raw STOMP client. Frames are NUL terminated; content-length is honoured."""

    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=TIMEOUT)
        self.buf = b""

    def send_raw(self, data):
        self.sock.sendall(data)

    def send(self, frame):
        self.send_raw((frame.encode() if isinstance(frame, str) else frame) + b"\0")

    def _fill(self):
        chunk = self.sock.recv(65536)
        if not chunk:
            raise EOFError("connection closed")
        self.buf += chunk

    def recv_frame(self):
        """Next frame as raw bytes without the terminating NUL (leading EOLs dropped)."""
        while True:
            self.buf = self.buf.lstrip(b"\r\n")
            first_nul = self.buf.find(b"\0")
            # headers never contain NUL: a blank line after the first NUL belongs to a later frame
            head_end = self.buf.find(b"\n\n", 0, first_nul if first_nul >= 0 else len(self.buf))
            if head_end >= 0:
                length = None
                for line in self.buf[:head_end].split(b"\n")[1:]:
                    if line.startswith(b"content-length:"):
                        length = int(line.split(b":", 1)[1])
                        break
                end = head_end + 2 + length if length is not None else self.buf.find(b"\0", head_end)
                if end >= 0 and len(self.buf) > end:
                    expect(self.buf[end:end + 1] == b"\0", f"frame not NUL terminated: {self.buf[:80]!r}")
                    frame, self.buf = self.buf[:end], self.buf[end + 1:]
                    return frame
            elif b"\0" in self.buf:  # malformed frame from the broker
                frame, self.buf = self.buf.split(b"\0", 1)
                return frame
            self._fill()

    def recv(self):
        return self.recv_frame().decode(errors="replace")

    def expect_eof(self):
        """The broker closed the connection (recv returns 0)."""
        try:
            while True:
                chunk = self.sock.recv(4096)
                if not chunk:
                    return
                self.buf += chunk
        except ConnectionResetError:
            return
        except socket.timeout:
            raise TestFailure("connection was not closed by the broker")

    def close(self):
        self.sock.close()


def headers_of(frame):
    head = frame.split("\n\n", 1)[0].split("\n")
    return dict(line.split(":", 1) for line in head[1:] if ":" in line)


def body_of(frame):
    return frame.split("\n\n", 1)[1]


def connected_client(port):
    c = Client(port)
    c.send("CONNECT\naccept-version:1.2\nhost:localhost\n\n")
    reply = c.recv()
    expect(reply.startswith("CONNECTED\n"), f"expected CONNECTED, got {reply[:60]!r}")
    return c


def subscribe(client, destination, sub_id, receipt=None):
    receipt = receipt or f"sub-{sub_id}"
    client.send(f"SUBSCRIBE\nid:{sub_id}\ndestination:{destination}\nreceipt:{receipt}\n\n")
    reply = client.recv()
    expect(reply.startswith("RECEIPT\n") and headers_of(reply).get("receipt-id") == receipt,
           f"expected RECEIPT {receipt}, got {reply[:60]!r}")


# --------------------------------------------------------------------------
# Scenarios
# --------------------------------------------------------------------------
def scenario_pubsub_round_trip(port):
    sub = connected_client(port)
    subscribe(sub, "/topic/rt", 1)
    pub = connected_client(port)
    pub.send("SEND\ndestination:/topic/rt\n\nhello tcp")
    msg = sub.recv()
    expect(msg.startswith("MESSAGE\n"), f"expected MESSAGE, got {msg[:60]!r}")
    h = headers_of(msg)
    expect(h.get("destination") == "/topic/rt" and h.get("subscription") == "1", f"headers {h}")
    expect(body_of(msg) == "hello tcp", f"body {body_of(msg)!r}")
    sub.close()
    pub.close()


def scenario_two_frames_in_one_write(port):
    c = connected_client(port)
    c.send_raw(b"SUBSCRIBE\nid:1\ndestination:/topic/two\nreceipt:a\n\n\0"
               b"SEND\ndestination:/topic/two\n\none write\0")
    expect(headers_of(c.recv()).get("receipt-id") == "a", "receipt of the first frame")
    expect(body_of(c.recv()) == "one write", "message of the second frame")
    c.close()


def scenario_malformed_frame_gets_error(port):
    c = connected_client(port)
    c.send("SEND\ndestination:/x\n")  # headers end without the blank line (used to crash the broker)
    reply = c.recv()
    expect(reply.startswith("ERROR\n"), f"expected ERROR, got {reply[:60]!r}")
    c.close()
    connected_client(port).close()  # still alive


def scenario_truncated_frames_loop(port):
    """Every prefix of valid frames, each on its own connection: the broker must survive."""
    frames = [
        "SUBSCRIBE\nid:1\ndestination:/t/*\nreceipt:r\n\n",
        "SEND\ndestination:/t/a\ncontent-length:5\n\nhello",
        "CONNECT\naccept-version:1.2\n\n",
    ]
    for frame in frames:
        for cut in range(len(frame) + 1):
            c = connected_client(port)
            c.send(frame[:cut])
            try:
                drain_until_receipt(c, "end-of-loop")
            except (EOFError, ConnectionResetError):
                pass  # ERROR closed the connection: everything was sent before the close
            c.close()
    connected_client(port).close()


def drain_until_receipt(client, receipt):
    """Reads every pending reply: the broker answers frames in order, so the receipt comes last."""
    client.send(f"SUBSCRIBE\nid:{receipt}\ndestination:/drain\nreceipt:{receipt}\n\n")
    while True:
        frame = client.recv()
        if frame.startswith("RECEIPT\n") and headers_of(frame).get("receipt-id") == receipt:
            return


def scenario_strict_command_and_header_names(port):
    for frame in ("SENDX\ndestination:/t\n\nbody", "SEND\ndestinationx:/t\n\nbody"):
        c = connected_client(port)
        c.send(frame)
        reply = c.recv()
        expect(reply.startswith("ERROR\n"), f"{frame!r}: expected ERROR, got {reply[:60]!r}")
        c.close()


def scenario_frame_split_at_every_offset(port):
    sub = connected_client(port)
    subscribe(sub, "/topic/split", 1)
    pub = connected_client(port)
    frame = b"SEND\ndestination:/topic/split\n\nsplit body\0"
    for cut in range(1, len(frame)):
        pub.send_raw(frame[:cut])
        time.sleep(0.003)  # separate TCP segments / reads
        pub.send_raw(frame[cut:])
        msg = sub.recv()
        expect(body_of(msg) == "split body", f"cut {cut}: {msg[:80]!r}")
    sub.close()
    pub.close()


def scenario_large_frame_spans_reads(port):
    """20 KB SEND: larger than the 16000 byte read buffer."""
    c = connected_client(port)
    subscribe(c, "/topic/large", 1)
    body = "".join(chr(ord("a") + i % 26) for i in range(20000))
    c.send(f"SEND\ndestination:/topic/large\n\n{body}")
    msg = c.recv()
    expect(body_of(msg) == body, f"large body mangled (len {len(body_of(msg))})")
    c.close()


def scenario_too_large_frame_gets_error_and_close(port):
    c = connected_client(port)
    c.send_raw(b"SEND\ndestination:/topic/huge\n\n" + b"h" * 1_100_000)  # over max_frame_size, no NUL yet
    reply = c.recv()
    expect(reply.startswith("ERROR\n") and "too large" in reply, f"expected ERROR, got {reply[:80]!r}")
    c.expect_eof()
    c.close()
    connected_client(port).close()


def scenario_body_with_nul_round_trips(port):
    c = connected_client(port)
    subscribe(c, "/topic/bin", 1)
    body = b"x\0y\0z"
    c.send(b"SEND\ndestination:/topic/bin\ncontent-length:5\n\n" + body)
    msg = c.recv_frame()
    head, got = msg.split(b"\n\n", 1)
    expect(got == body, f"body {got!r}")
    expect(b"\ncontent-length:5" in head, f"headers {head!r}")
    c.close()


def scenario_content_length_mismatch_gets_error(port):
    c = connected_client(port)
    c.send(b"SEND\ndestination:/topic/bin\ncontent-length:2\n\nabc")  # no NUL after 2 bytes
    reply = c.recv()
    expect(reply.startswith("ERROR\n"), f"expected ERROR, got {reply[:60]!r}")
    c.expect_eof()
    c.close()


SCENARIOS = [
    scenario_pubsub_round_trip,
    scenario_two_frames_in_one_write,
    scenario_malformed_frame_gets_error,
    scenario_truncated_frames_loop,
    scenario_strict_command_and_header_names,
    scenario_frame_split_at_every_offset,
    scenario_large_frame_spans_reads,
    scenario_too_large_frame_gets_error_and_close,
    scenario_body_with_nul_round_trips,
    scenario_content_length_mismatch_gets_error,
]


# --------------------------------------------------------------------------
# Runner
# --------------------------------------------------------------------------
def wait_for_port(port, broker, deadline=10.0):
    end = time.time() + deadline
    while time.time() < end:
        if broker is not None and broker.poll() is not None:
            raise TestFailure(f"broker exited with {broker.returncode} before listening")
        try:
            socket.create_connection(("127.0.0.1", port), timeout=0.5).close()
            return
        except OSError:
            time.sleep(0.1)
    raise TestFailure(f"broker did not listen on {port}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=0, help="0: pick a free port (default)")
    parser.add_argument("--broker", help="broker binary to start (otherwise one must be running on --port)")
    parser.add_argument("--log", default="e2e_stomp_broker.log")
    args = parser.parse_args()
    port = args.port or free_port()

    broker = None
    log = None
    if args.broker:
        log = open(args.log, "w")
        broker = subprocess.Popen([args.broker, f"port={port}", "processors=4", "TTL=5000"],
                                  stdout=log, stderr=subprocess.STDOUT)
    failures = 0
    try:
        wait_for_port(port, broker)
        for scenario in SCENARIOS:
            try:
                scenario(port)
                print(f"PASS {scenario.__name__}")
            except Exception as error:  # noqa: BLE001 - report every scenario
                failures += 1
                print(f"FAIL {scenario.__name__}: {type(error).__name__}: {error}")
            if broker is not None and broker.poll() is not None:
                failures += 1
                print(f"FAIL broker exited with {broker.returncode} after {scenario.__name__}")
                break
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

    print(f"stomp e2e: {len(SCENARIOS)} scenarios, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
