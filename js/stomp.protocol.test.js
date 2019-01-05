#!/usr/bin/env node

console.log("Run stompbroker with DEBUG_MESSAGE_ID!");

const TEST_DATA = [
  "Initial session-connected-size",
  "DIAG\n\nsession-connected-size",
  "DIAG\ncontent-type:text/plain\nsession-connected-size:0\n",

  "Initial session-size",
  "DIAG\n\nsession-size",
  "DIAG\ncontent-type:text/plain\nsession-size:1\n",

  "session-encoded-size",
  "DIAG\n\nsession-encoded-size",
  "DIAG\ncontent-type:text/plain\nsession-encoded-size:0\n",

  "Stomp ver. 1.2 connect frame",
  "STOMP\naccept-version:1.2\nhost:localhost\n\n",
  check_connected,

  "Session size increment",
  "DIAG\n\nsession-connected-size",
  "DIAG\ncontent-type:text/plain\nsession-connected-size:1\n",

  "1 client connected session-size",
  "DIAG\n\nsession-size",
  "DIAG\ncontent-type:text/plain\nsession-size:1\n",

  "Disconnect with receipt",
  "DISCONNECT\nreceipt:77\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n",

  "Stomp ver. 1.2 legacy connect frame",
  "CONNECT\naccept-version:1.2\nhost:localhost\n\n",
  check_connected,
  "Disconnect with receipt",
  "DISCONNECT\nreceipt:77\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n",

  "Stomp ver. 1.1. connect frame",
  "CONNECT\naccept-version:1.1\nhost:localhost\n\n",
  check_connected,

  "Disconnect with receipt",
  "DISCONNECT\nreceipt:77\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n",

  "Connect frame with CRLF",
  "CONNECT\r\naccept-version:1.1\nhost:localhost\n\n",
  check_connected,

  "SUBSCRIBE with missing destination",
  "SUBSCRIBE\nid:1\nreceipt:m-a\n",
  "ERROR\ncontent-type:text/plain\ncontent-length:23\n\nNo destination defined!\n",

  "SUBSCRIBE with missing id",
  "SUBSCRIBE\ndestination:/*\nreceipt:m-a\n",
  "ERROR\ncontent-type:text/plain\ncontent-length:14\n\nNo id defined!\n",


  "UNSUBSCRIBE with missing destination",
  "UNSUBSCRIBE\nid:1\nreceipt:m-a\n",
  "ERROR\ncontent-type:text/plain\ncontent-length:23\n\nNo destination defined!\n",

  "UNSUBSCRIBE with missing id",
  "UNSUBSCRIBE\ndestination:/*\nreceipt:m-a\n",
  "ERROR\ncontent-type:text/plain\ncontent-length:14\n\nNo id defined!\n",

  "Disconnect with receipt",
  "DISCONNECT\nreceipt:77\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n",

  "0> Connect frame ignore invalid header/body",
  "CONNECT\naccept-version:1.1\nhost:localhost\nbody\n",
  check_connected,

  "0> Subscribe * frame with receipt",
  "SUBSCRIBE\ndestination:/*\nid:1\nreceipt:m-99\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:m-99\n",

  "0> increases pubsub-size",
  "DIAG\n\npubsub-size",
  "DIAG\ncontent-type:text/plain\npubsub-size:1\n",

  "0> Subscribe to topic",
  "SUBSCRIBE\ndestination:/a_topic\nid:1\nreceipt:m-a\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:m-a\n",

  "0> Increased pubsub-size",
  "DIAG\n\npubsub-size",
  "DIAG\ncontent-type:text/plain\npubsub-size:2\n",

  "0> Diag message for listing subscriptions",
  "DIAG\n\nsubs",
  "DIAG\ncontent-type:text/plain\nsubs:/*,/a_topic\n",

  "0> Unsubscribe frame",
  "UNSUBSCRIBE\ndestination:/a_topic\nid:1\nreceipt:m-au\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:m-au\n",

  "0> Decreases pubsub-size",
  "DIAG\n\npubsub-size",
  "DIAG\ncontent-type:text/plain\npubsub-size:1\n",

  "0> Disconnect with receipt (removes subscription)",
  "DISCONNECT\nreceipt:78\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:78\n",

  "0> Decrased pubsub-size",
  "DIAG\n\npubsub-size",
  "DIAG\ncontent-type:text/plain\npubsub-size:0\n",

  "1> Connect",
  "STOMP\naccept-version:1.2\nhost:localhost\n\n",
  check_connected,

  "1> Wildcard subscription",
  "SUBSCRIBE\ndestination:/queue/*\nid:sub-1\nreceipt:m-queue/a\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:m-queue/a\n",

  "1> MESSAGE frame arrives with a specific destination",
  "SEND\ndestination:/queue/a\ncontent-type:text/plain\n\nhello queue a\n",
  "MESSAGE\ndestination:/queue/a\nmessage-id:0\nsubscription:sub-1\ncontent-type:text/plain\ncontent-length:14\n\nhello queue a\n",

  "1> SEND frame arrives with a wildcard destination",
  "SEND\ndestination:/queue/\ncontent-type:text/plain\n\nhello queue *\n",
  "MESSAGE\ndestination:/queue/\nmessage-id:0\nsubscription:sub-1\ncontent-type:text/plain\ncontent-length:14\n\nhello queue *\n",

  "1> Custom header is sent with message",
  "SEND\ndestination:/queue/abc\ncontent-type:text/plain\ncustom-header:test\n\nhello queue *\n",
  "MESSAGE\ndestination:/queue/abc\nmessage-id:0\nsubscription:sub-1\ncontent-type:text/plain\ncustom-header:test\ncontent-length:14\n\nhello queue *\n",

  "1> Disconnect",
  "DISCONNECT\nreceipt:77\n",
  "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n",

  "2> Invalid session should get ERROR frame on SEND",
  "SEND\ndestination:/i_am_not_connected\ncontent-type:text/plain\n\nhello there\n",
  'ERROR\ncontent-type:text/plain\ncontent-length:14\n\nNot connected!\n',

  "2> Invalid session should get ERROR frame on DISCONNECT",
  "DISCONNECT\n",
  'ERROR\ncontent-type:text/plain\ncontent-length:48\n\nCan not disconnect, client is not connected yet!\n',

  "2> Invalid session should get ERROR frame on SUBSCRIBE",
  "SUBSCRIBE\ndestination:/queue/*\nid:1\nreceipt:m-queue/a\n",
  'ERROR\ncontent-type:text/plain\ncontent-length:14\n\nNot connected!\n',

  "2> Invalid session should get ERROR frame on UNSUBSCRIBE",
  "UNSUBSCRIBE\ndestination:/a_topic\nid:1\nreceipt:m-au\n",
  'ERROR\ncontent-type:text/plain\ncontent-length:14\n\nNot connected!\n',

  "3> Invalid message should get ERROR frame",
  "INVALID\n",
  'ERROR\ncontent-type:text/plain\ncontent-length:16\n\nInvalid message!\n',

];

function check_connected(resp) {
  if (!resp.match(/CONNECTED/))
    return `Could not connect! Response frame was:\n${resp}`;
}

const net = require("net");
let conn, connStatus;

const connectDurable = port => {
  console.log("connecting...");
  const retry = () => setTimeout(() => connectDurable(port), 5000);
  conn = net.connect(port);
  conn.once("ready", () => {
    console.log("connected.");
    connStatus = "OK";
  });
  conn.once("error", e => {
    console.error(e.toString());
    connStatus = "ERROR";
  });
  conn.once("close", retry);
};

const request = m =>
  new Promise((resolve, reject) => {
    conn.once("data", resolve);
    conn.write(m);
    setTimeout(() => reject("timeout"), 100);
  });

const awaitResponse = async (requestMessage, responseMessage) => {
  const response = await request(requestMessage);

  if (typeof responseMessage == "function") {
    const ret = responseMessage(response.toString());
    if (typeof ret == "string") {
      throw `Invalid response:\n${ret}\n`;
    }
  } else if (
    response.toString().replace("\0", "") !=
    responseMessage.toString().replace("\0", "")
  ) {
    throw `Invalid response:\nExpected to get:\n${responseMessage}\nGot:\n${response}--\n`;
  }
  return "SUCC";
};

let cursor = 0;

const delay = async d => new Promise(resolve => setTimeout(resolve, d));

// TODO change it to for await of :)
const runSingleTest = async () => {
  let resp = await awaitResponse(TEST_DATA[cursor + 1], TEST_DATA[cursor + 2])
    .then(ok => {
      console.log(`${ok}[${TEST_DATA[cursor]}]`);
      return ok;
    })
    .catch(err =>
      console.log(`================\nERROR: ${TEST_DATA[cursor]}\n${err}\n\n`)
    );
  if (resp === "SUCC") {
    cursor += 3;
    if (cursor >= TEST_DATA.length) {
      cursor = 0;
      console.log("All the tests done. Restarting in 10 seconds...");
      await delay(10000);
    }
  } else {
    console.log("Test failed. Restarting in 10 seconds...");
    await delay(10000);
  }
  return resp;
};

const error = message => {
  console.log(message);
  console.log("Error at test execution, retrying in 5 secs....");
  setTimeout(run, 5000);
};
const run = () => {
  try {
    if (connStatus === "OK") {
      runSingleTest().then(() => {
        setTimeout(run, 0);
      });
    } else error("NOT_CONNECTED");
  } catch (err) {
    error(err);
  }
};

console.log("Started");
connectDurable(3490);
run();
