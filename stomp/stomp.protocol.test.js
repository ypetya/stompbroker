#!/usr/bin/env node

const TEST_DATA = [
    "STOMP\naccept-version:1.2\nhost:localhost\n\n", "CONNECTED\nversion:1.2\n\n",
    "CONNECT\naccept-version:1.2\nhost:localhost\n\n", "CONNECTED\nversion:1.2\n\n",
    "CONNECT\naccept-version:1.1\nhost:localhost\n\n", "CONNECTED\nversion:1.2\n\n",
    "CONNECT\r\naccept-version:1.1\nhost:localhost\n\n", "CONNECTED\nversion:1.2\n\n",
    "CONNECT\naccept-version:1.1\nhost:localhost\nbody\n", "ERROR\ncontent-length:15\ncontent-type:text;charser=utf-8\n\ninvalid message"
];

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

    if (response != responseMessage) {
        throw(`Invalid response:\nExpected to get:${responseMessage}\nGot:\n${response}--\n`);
    }
    return 'SUCC';
}

let cursor = 0;

const runSingleTest = async () => {
    let resp = await awaitResponse(TEST_DATA[cursor], TEST_DATA[cursor + 1])
            .then(ok => console.log(`${ok}[${cursor}]`))
            .catch(err => console.log(`ERROR at index ${cursor} ================\n${err}\n\n`));
    if (resp === 'SUCC') {
        cursor += 2;
        if(cursor > TET_DATA.length) cursor = 0;
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
            console.log('Running test...');
            runSingleTest().then(() => {
                console.log('Done. Restarting in 5sec');
                setTimeout(run, 5000);
            })
        } else error('NOT_CONNECTED');

    } catch (err) {
        error(err);
    }
}

console.log('Started');
connectDurable(3490);
run();
