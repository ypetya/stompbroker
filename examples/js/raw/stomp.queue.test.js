#!/usr/bin/env node

const net = require('net');

const conn = net.connect(3490);
conn.setNoDelay(true);
conn.once('ready', () => {
    conn.write("STOMP\naccept-version:1.2\nhost:localhost\n\n");
    conn.once('data', d => {
        const msg = d.toString();
        if (msg.indexOf('CONNECTED') == 0) {
            console.log('connected.');
            runTest();
        }
    });
});
conn.once('error', (e) => {
    console.error(e.toString());
});
conn.once('close', () => {
    console.log('Err');
    process.exit(1);
});
let arrived = 0, err = 0, others = [];
const SUBSCRIPTIONS = 1000, MESSAGES = 1000;
const TOTAL = SUBSCRIPTIONS * MESSAGES;
conn.on('data', (d) => {
    d.toString().split('\0').forEach(x => {

        if (x.indexOf('MESSAGE') == 0) {
            arrived++;
            console.log(`Arrived #${arrived}, remaining: ${TOTAL - arrived}, err: ${err}, other:${others.length}\n`);
        } else if (x.indexOf('ERROR') == 0) {
            err++;
        } else if(x.length>3 && x.length < 80) {
            console.log(x);
            others.push(x);
        }

    });
});


const request = m => new Promise((resolve, reject) => {
    setImmediate(() => conn.write(m));
    setTimeout(resolve, 0);
});
function runTest() {
    createSubscriptions(SUBSCRIPTIONS).then(
        async () => {
            for (var i = 0; i < MESSAGES; i++) {
                await request(`SEND\ndestination:/queue/${i}\ncontent-type:text/plain\n\n{\"content\":\"Funky ${i}!\"}\0`);
                //setImmediate(sendMessage);
            }
        });
    setTimeout(getErronousFrames, 70000);
}

async function getErronousFrames() {
    await request(`SUBSCRIBE\ndestination:/ready\nid:1\n\0`);
    conn.on('data', (d) => {
        let m = d.toString()
        if (m.indexOf('MESSAGE') == 0) { console.log(others.join(';')); }
    });
    await request(`SEND\ndestination:/ready\ncontent-type:text/plain\n\nprint errors\0`);

}

async function createSubscriptions(i) {
    for (let ix = 0; ix < i; ix++) {
        await request(`SUBSCRIBE\ndestination:/queue/*\nid:1\n\0`);
    }

    return true;
}

function sendMessage() {
    conn.write("SEND\ndestination:/*\ncontent-type:text/plain\n\n{\"content\":\"Funky!\"}\0");
}
