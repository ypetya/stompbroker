#!/usr/bin/env node
/*
 * Raw STOMP 1.2 protocol test over TCP. Runs the steps once and exits with
 * 0 (all passed) or 1. Expects a freshly started broker (the DIAG counters
 * at the start assume no other clients):
 *
 *   node examples/js/raw/stomp.protocol.test.js [port]    (default 3490)
 *   make test-e2e     # starts a broker on a free port and runs this test
 *
 * A step is [description, frame to send (without NUL), expectation]:
 * - a frame string: compared by command, header set and body
 *   (the value of message-id is not compared, it is a running counter),
 * - a function(frame): returns an error text, or nothing if the frame is fine.
 * - CLOSED: nothing is sent, the broker must close the connection.
 * RECONNECT as a step opens a fresh connection. The broker closes the
 * connection after every ERROR and after DISCONNECT (STOMP 1.2).
 */
const net = require("net");

const PORT = Number(process.argv[2] || 3490);
const TIMEOUT_MS = 2000;
const RECONNECT = "RECONNECT";
const CLOSED = "CLOSED";
const closed = what => [`${what}: connection is closed`, null, CLOSED];

const connected = version => frame =>
  frame.command !== "CONNECTED"
    ? `expected CONNECTED, got ${frame.raw}`
    : version && frame.headers.version !== version
      ? `expected version ${version}, got ${frame.headers.version}`
      : undefined;

const error = (reason, receiptId) => frame =>
  frame.command !== "ERROR"
    ? `expected ERROR, got ${frame.raw}`
    : !frame.body.includes(reason)
      ? `expected ERROR "${reason}", got "${frame.body}"`
      : receiptId !== undefined && frame.headers["receipt-id"] !== receiptId
        ? `expected receipt-id ${receiptId}, got ${frame.headers["receipt-id"]}`
        : undefined;

const receipt = id => `RECEIPT\nreceipt-id:${id}\n\n`;
const diag = (key, value) => `DIAG\ncontent-type:text/plain\n${key}:${value}\n\n`;

const STEPS = [
  ["Initial session-connected-size", "DIAG\n\nsession-connected-size", diag("session-connected-size", 0)],
  ["Initial session-size", "DIAG\n\nsession-size", diag("session-size", 1)],
  ["Initial session-encoded-size", "DIAG\n\nsession-encoded-size", diag("session-encoded-size", 0)],

  ["STOMP 1.2 STOMP frame", "STOMP\naccept-version:1.2\nhost:localhost\n\n", connected("1.2")],
  ["Session size increment", "DIAG\n\nsession-connected-size", diag("session-connected-size", 1)],
  ["Disconnect with receipt", "DISCONNECT\nreceipt:77\n\n", receipt(77)],
  closed("DISCONNECT"),
  RECONNECT,

  ["STOMP 1.2 CONNECT frame", "CONNECT\naccept-version:1.2\nhost:localhost\n\n", connected("1.2")],
  ["Disconnect with receipt", "DISCONNECT\nreceipt:77\n\n", receipt(77)],
  closed("DISCONNECT"),
  RECONNECT,

  ["STOMP 1.1 CONNECT frame", "CONNECT\naccept-version:1.1\nhost:localhost\n\n", connected("1.1")],
  ["Disconnect with receipt", "DISCONNECT\nreceipt:77\n\n", receipt(77)],
  closed("DISCONNECT"),
  RECONNECT,

  ["CONNECT without accept-version is STOMP 1.0", "CONNECT\nhost:localhost\n\n", connected("1.0")],
  ["Disconnect without receipt", "DISCONNECT\n\n", CLOSED],
  RECONNECT,

  ["Unsupported version", "CONNECT\naccept-version:2.0\nhost:localhost\n\n", error("Supported protocol versions are 1.0,1.1,1.2")],
  closed("ERROR"),
  RECONNECT,

  ["CONNECT frame with CRLF", "CONNECT\r\naccept-version:1.1\r\nhost:localhost\r\n\r\n", connected("1.1")],
  ["UNSUBSCRIBE needs only an id", "UNSUBSCRIBE\nid:1\nreceipt:m-u\n\n", receipt("m-u")],
  ["SUBSCRIBE with missing destination", "SUBSCRIBE\nid:1\nreceipt:m-a\n\n", error("No destination defined!", "m-a")],
  closed("ERROR"),
  RECONNECT,
  ["Connect", "CONNECT\naccept-version:1.2\nhost:localhost\n\n", connected("1.2")],
  ["SUBSCRIBE with missing id", "SUBSCRIBE\ndestination:/*\nreceipt:m-b\n\n", error("No id defined!", "m-b")],
  closed("ERROR"),
  RECONNECT,
  ["Connect", "CONNECT\naccept-version:1.2\nhost:localhost\n\n", connected("1.2")],
  ["UNSUBSCRIBE with missing id", "UNSUBSCRIBE\ndestination:/*\nreceipt:m-c\n\n", error("No id defined!", "m-c")],
  closed("ERROR"),
  RECONNECT,

  ["Header line without ':' is an error", "CONNECT\naccept-version:1.1\nhost:localhost\nbody\n\n", error("header line without ':'")],
  closed("ERROR"),
  RECONNECT,

  ["0> Connect", "CONNECT\naccept-version:1.2\nhost:localhost\n\n", connected("1.2")],
  ["0> Subscribe * frame with receipt", "SUBSCRIBE\ndestination:/*\nid:1\nreceipt:m-99\n\n", receipt("m-99")],
  ["0> increases pubsub-size", "DIAG\n\npubsub-size", diag("pubsub-size", 1)],
  ["0> Subscribe to topic", "SUBSCRIBE\ndestination:/a_topic\nid:2\nreceipt:m-a\n\n", receipt("m-a")],
  ["0> Increased pubsub-size", "DIAG\n\npubsub-size", diag("pubsub-size", 2)],
  ["0> Diag message for listing subscriptions", "DIAG\n\nsubs", diag("subs", "/*,/a_topic")],
  ["0> Unsubscribe frame", "UNSUBSCRIBE\nid:2\nreceipt:m-au\n\n", receipt("m-au")],
  ["0> Decreases pubsub-size", "DIAG\n\npubsub-size", diag("pubsub-size", 1)],
  ["0> Duplicate subscription id", "SUBSCRIBE\ndestination:/b_topic\nid:1\nreceipt:dup\n\n", error("Subscription id is already in use!", "dup")],
  closed("ERROR"),
  RECONNECT,
  ["0> ERROR removed the subscriptions", "DIAG\n\npubsub-size", diag("pubsub-size", 0)],

  ["1> Connect", "STOMP\naccept-version:1.2\nhost:localhost\n\n", connected("1.2")],
  ["1> Wildcard subscription", "SUBSCRIBE\ndestination:/queue/*\nid:sub-1\nreceipt:m-queue/a\n\n", receipt("m-queue/a")],
  ["1> MESSAGE frame arrives with a specific destination",
    "SEND\ndestination:/queue/a\ncontent-type:text/plain\n\nhello queue a\n",
    "MESSAGE\ndestination:/queue/a\nmessage-id:0\nsubscription:sub-1\ncontent-type:text/plain\ncontent-length:14\n\nhello queue a\n"],
  ["1> SEND frame arrives with a wildcard destination",
    "SEND\ndestination:/queue/\ncontent-type:text/plain\n\nhello queue *\n",
    "MESSAGE\ndestination:/queue/\nmessage-id:0\nsubscription:sub-1\ncontent-type:text/plain\ncontent-length:14\n\nhello queue *\n"],
  ["1> Custom header is sent with message",
    "SEND\ndestination:/queue/abc\ncontent-type:text/plain\ncustom-header:test\n\nhello queue *\n",
    "MESSAGE\ndestination:/queue/abc\nmessage-id:0\nsubscription:sub-1\ncontent-type:text/plain\ncustom-header:test\ncontent-length:14\n\nhello queue *\n"],
  ["1> Body starting with HTTP is still NUL terminated",
    "SEND\ndestination:/queue/http\n\nHTTP/1.1 200 OK",
    "MESSAGE\ndestination:/queue/http\nmessage-id:0\nsubscription:sub-1\ncontent-type:text/plain\ncontent-length:15\n\nHTTP/1.1 200 OK"],
  ["1> Disconnect (removes subscription)", "DISCONNECT\nreceipt:78\n\n", receipt(78)],
  closed("DISCONNECT"),
  RECONNECT,
  ["1> Decreased pubsub-size", "DIAG\n\npubsub-size", diag("pubsub-size", 0)],

  ["2> Not connected: SEND", "SEND\ndestination:/i_am_not_connected\ncontent-type:text/plain\n\nhello there\n", error("Not connected!")],
  closed("ERROR"),
  RECONNECT,
  ["2> Not connected: DISCONNECT", "DISCONNECT\n\n", error("Can not disconnect, client is not connected yet!")],
  closed("ERROR"),
  RECONNECT,
  ["2> Not connected: SUBSCRIBE", "SUBSCRIBE\ndestination:/queue/*\nid:1\nreceipt:m-queue/a\n\n", error("Not connected!", "m-queue/a")],
  closed("ERROR"),
  RECONNECT,
  ["2> Not connected: UNSUBSCRIBE", "UNSUBSCRIBE\nid:1\nreceipt:m-au\n\n", error("Not connected!", "m-au")],
  closed("ERROR"),
  RECONNECT,

  ["3> Invalid command", "INVALID\n\n", error("Invalid message!")],
  closed("ERROR"),
];

/* ------------------------------------------------------------------------
 * Frame reader: NUL terminated frames, content-length honoured
 * ---------------------------------------------------------------------- */
function parseFrame(raw) {
  const text = raw.toString();
  const split = text.indexOf("\n\n");
  const head = (split < 0 ? text : text.slice(0, split)).split("\n");
  const headers = {};
  for (const line of head.slice(1)) {
    const colon = line.indexOf(":");
    if (colon > 0 && !(line.slice(0, colon) in headers)) headers[line.slice(0, colon)] = line.slice(colon + 1);
  }
  return { raw: text, command: head[0], headers, body: split < 0 ? "" : text.slice(split + 2) };
}

/** @return [frame bytes, rest] or null when buf holds no complete frame */
function nextFrame(buf) {
  let start = 0;
  while (start < buf.length && (buf[start] === 0x0a || buf[start] === 0x0d)) start++;
  buf = buf.subarray(start);
  const nul = buf.indexOf(0);
  const headEnd = buf.subarray(0, nul < 0 ? buf.length : nul).indexOf("\n\n");
  let end = nul;
  if (headEnd >= 0) {
    const length = /\ncontent-length:(\d+)/.exec(buf.subarray(0, headEnd).toString());
    if (length) end = headEnd + 2 + Number(length[1]);
  }
  if (end < 0 || buf.length <= end) return null;
  if (buf[end] !== 0) throw new Error(`frame not NUL terminated: ${buf.subarray(0, end + 1)}`);
  return [buf.subarray(0, end), buf.subarray(end + 1)];
}

class Connection {
  constructor(port) {
    this.buf = Buffer.alloc(0);
    this.waiters = [];
    this.closed = false;
    this.socket = net.connect(port, "127.0.0.1");
    this.ready = new Promise((resolve, reject) => {
      this.socket.once("connect", resolve);
      this.socket.once("error", reject);
    });
    this.socket.on("data", data => {
      this.buf = Buffer.concat([this.buf, data]);
      this.flush();
    });
    this.closedPromise = new Promise(resolve => this.socket.on("close", resolve));
    this.socket.on("close", () => {
      this.closed = true;
      this.flush();
    });
    this.socket.on("error", () => {});
  }

  flush() {
    while (this.waiters.length > 0) {
      const next = nextFrame(this.buf);
      if (next === null) {
        if (this.closed) this.waiters.shift().reject(new Error("connection closed"));
        else return;
      } else {
        this.buf = next[1];
        this.waiters.shift().resolve(parseFrame(next[0]));
      }
    }
  }

  request(frame) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error("timeout")), TIMEOUT_MS);
      this.waiters.push({
        resolve: f => { clearTimeout(timer); resolve(f); },
        reject: e => { clearTimeout(timer); reject(e); },
      });
      this.socket.write(frame + "\0");
      this.flush();
    });
  }

  /** Resolves when the broker closes the connection; rejects after the timeout. */
  waitClosed() {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error("connection was not closed")), TIMEOUT_MS);
      this.closedPromise.then(() => { clearTimeout(timer); resolve(); });
    });
  }

  close() {
    this.socket.destroy();
  }
}

function compare(expected, actual) {
  const exp = parseFrame(expected);
  const sameHeaders = (a, b) =>
    Object.keys(a).length === Object.keys(b).length &&
    Object.keys(a).every(k => k === "message-id" ? k in b : a[k] === b[k]);
  if (exp.command === actual.command && exp.body === actual.body && sameHeaders(exp.headers, actual.headers)) return undefined;
  return `expected:\n${expected}\ngot:\n${actual.raw}`;
}

async function main() {
  let conn = new Connection(PORT);
  await conn.ready;
  let failures = 0;
  for (const step of STEPS) {
    if (step === RECONNECT) {
      conn.close();
      conn = new Connection(PORT);
      await conn.ready;
      continue;
    }
    const [name, frame, expectation] = step;
    let problem;
    try {
      if (expectation === CLOSED) {
        if (frame !== null) conn.socket.write(frame + "\0");
        await conn.waitClosed();
      } else {
        const reply = await conn.request(frame);
        problem = typeof expectation === "function" ? expectation(reply) : compare(expectation, reply);
      }
    } catch (e) {
      problem = e.message;
    }
    if (problem) failures++;
    console.log(`${problem ? "FAIL" : "PASS"} ${name}${problem ? `\n  ${problem.replace(/\n/g, "\n  ")}` : ""}`);
  }
  conn.close();
  console.log(`js protocol test: ${STEPS.filter(s => s !== RECONNECT).length} steps, ${failures} failed`);
  process.exit(failures ? 1 : 0);
}

main().catch(e => {
  console.error(e);
  process.exit(1);
});
