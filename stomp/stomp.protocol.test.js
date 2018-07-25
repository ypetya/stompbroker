#!/usr/bin/env node

const TEST_DATA_FOR_CONNECT = [
    "DIAG\n\nsession-size",
    "DIAG\ncontent-type:text/plain\nsession-size:0\n",
    "STOMP\naccept-version:1.2\nhost:localhost\n\n",
    check_connected,
    "DIAG\n\nsession-size",
    "DIAG\ncontent-type:text/plain\nsession-size:1\n",
    "DISCONNECT\nreceipt:77\n",
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n",
    "CONNECT\naccept-version:1.2\nhost:localhost\n\n",
    check_connected,
    "DISCONNECT\nreceipt:77\n",
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n",
    "CONNECT\naccept-version:1.1\nhost:localhost\n\n",
    check_connected,
    "DISCONNECT\nreceipt:77\n",
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n",
    "CONNECT\r\naccept-version:1.1\nhost:localhost\n\n",
    check_connected,
    "DISCONNECT\nreceipt:77\n",
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n",
    "CONNECT\naccept-version:1.1\nhost:localhost\nbody\n",
    check_connected,
    "SUBSCRIBE\ndestination:/*\nid:1\nreceipt:m-99\n",
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:m-99\n",
    "DIAG\n\npubsub-size",
    "DIAG\ncontent-type:text/plain\npubsub-size:1\n",
    "SUBSCRIBE\ndestination:/a_topic\nid:1\nreceipt:m-a\n",
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:m-a\n",
    "DIAG\n\npubsub-size",
    "DIAG\ncontent-type:text/plain\npubsub-size:2\n",
    "UNSUBSCRIBE\ndestination:/a_topic\nid:1\nreceipt:m-au\n",
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:m-au\n",
    "DIAG\n\npubsub-size",
    "DIAG\ncontent-type:text/plain\npubsub-size:1\n",
    "DISCONNECT\nreceipt:78\n", // at this point subscription should be removed!
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:78\n",
    "DIAG\n\npubsub-size",
    "DIAG\ncontent-type:text/plain\npubsub-size:0\n",
    "STOMP\naccept-version:1.2\nhost:localhost\n\n",
    check_connected,
    "SUBSCRIBE\ndestination:/queue/a\nid:1\nreceipt:m-queue/a\n",
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:m-queue/a\n",
    "SEND\ndestination:/queue/a\ncontent-type:text/plain\n\nhello queue a\n",
    "MESSAGE\ncontent-type:text/plain\ncontent-length:14\nsubscription:1\nmessage-id:0\ndestination:/queue/a\n\nhello queue a\n",
    "SEND\ndestination:/queue*\ncontent-type:text/plain\n\nhello queue *\n",
    "MESSAGE\ncontent-type:text/plain\ncontent-length:14\nsubscription:1\nmessage-id:0\ndestination:/queue*\n\nhello queue *\n",
    "DISCONNECT\nreceipt:77\n",
    "RECEIPT\ncontent-type:text/plain\nreceipt-id:77\n"
 ];

function check_connected(resp) {
    if (!resp.match(/CONNECTED/))
        return `Could not connect! Response frame was:\n${resp}`;
}

const net = require('net');
let conn, connStatus;

const connectDurable = port => {
    console.log('connecting...');
    const retry = () => setTimeout(() => connectDurable(port), 5000)
    conn = net.connect(port);
    conn.once('ready', () => {
        console.log('connected.');
        connStatus = 'OK';
    });
    conn.once('error', (e) => {
        console.error(e.toString());
        connStatus = 'ERROR';
    });
    conn.once('close', retry);
}

const request = m => new Promise((resolve, reject) => {
        conn.once('data', resolve)
        conn.write(m);
        setTimeout(() => reject('timeout'), 100);
    });

const awaitResponse = async (requestMessage, responseMessage) => {
    const response = await request(requestMessage);

    if (typeof (responseMessage) == 'function') {
        const ret = responseMessage(response.toString());
        if (typeof (ret) == 'string') {
            throw(`Invalid response:\n${ret}\n`);
        }
    } else if (response != responseMessage) {
        throw(`Invalid response:\nExpected to get:\n${responseMessage}\nGot:\n${response}--\n`);
    }
    return 'SUCC';
}

let cursor = 0;

const delay = async (d) => new Promise(resolve => setTimeout(resolve, d));

const runSingleTest = async () => {
    let resp = await awaitResponse(TEST_DATA_FOR_CONNECT[cursor], TEST_DATA_FOR_CONNECT[cursor + 1])
            .then(ok => {
                console.log(`${ok}[${cursor}]`);
                return ok;
            })
            .catch(err => console.log(`ERROR at index ${cursor} ================\n${err}\n\n`));
    if (resp === 'SUCC') {
        cursor += 2;
        if (cursor >= TEST_DATA_FOR_CONNECT.length) {
            cursor = 0;
            console.log('All the tests done. Restarting in 10 seconds...');
            await delay(10000);
        }
    } else {
        console.log('Test failed. Restarting in 10 seconds...');
        await delay(10000);
    }
    return resp;
}

const error = message => {
    console.log(message);
    console.log('Error at test execution, retrying in 5 secs....');
    setTimeout(run, 5000);
}
const run = () => {
    try {

        if (connStatus === 'OK') {
            runSingleTest().then(() => {
                setTimeout(run, 0);
            })
        } else
            error('NOT_CONNECTED');

    } catch (err) {
        error(err);
    }
}

console.log('Started');
connectDurable(3490);
run();
