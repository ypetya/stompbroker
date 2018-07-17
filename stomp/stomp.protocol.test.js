#!/usr/bin/env node

const TEST_DATA = [
    "STOMP\naccept-version:1.2\nhost:localhost\n\n", 
        check_connected,
    "CONNECT\naccept-version:1.2\nhost:localhost\n\n",
        check_connected,
    "CONNECT\naccept-version:1.1\nhost:localhost\n\n",
        check_connected,
    "CONNECT\r\naccept-version:1.1\nhost:localhost\n\n",
        check_connected,
    "CONNECT\naccept-version:1.1\nhost:localhost\nbody\n",
        check_connected,
    
];

function check_connected(resp) {
    if(!resp.match(/CONNECTED/)) return "Not connected!";
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

    if(typeof(responseMessage)=='function'){
        const ret = responseMessage(response.toString());
        if(typeof(ret)=='string') {
            throw(`Invalid response\n:${ret}\n`);
        }
    } else if (response != responseMessage) {
        throw(`Invalid response:\nExpected to get:${responseMessage}\nGot:\n${response}--\n`);
    }
    return 'SUCC';
}

let cursor = 0;

const delay = async (d) => new Promise(resolve=> setTimeout(resolve,d));

const runSingleTest = async () => {
    let resp = await awaitResponse(TEST_DATA[cursor], TEST_DATA[cursor + 1])
            .then(ok =>{ console.log(`${ok}[${cursor}]`); return ok;})
            .catch(err => console.log(`ERROR at index ${cursor} ================\n${err}\n\n`));
    if (resp === 'SUCC') {
        cursor += 2;
        if(cursor >= TEST_DATA.length){
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
        } else error('NOT_CONNECTED');

    } catch (err) {
        error(err);
    }
}

console.log('Started');
connectDurable(3490);
run();
