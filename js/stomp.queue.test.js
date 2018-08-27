#!/usr/bin/env node

const net = require('net');

const conn = net.connect(3490);
conn.setNoDelay(true);
conn.once('ready', () => {
    conn.write("STOMP\naccept-version:1.2\nhost:localhost\n\n");
    conn.once('data', d => {
        if (d.toString().indexOf('CONNECTED') == 0) {
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
let arrived = 0;
conn.on('data', (d) => console.log(`Arrived ${arrived++}\n${d.toString()}\n`));


const request = m => new Promise((resolve, reject) => {
        setImmediate(() => conn.write(m));
        setTimeout(resolve, 0);
    });

function runTest() {
    createSubscriptions(1000).then(
            async () => {
        for (var i = 0; i < 1000; i++) {
            await request("SEND\ndestination:/*\ncontent-type:text/plain\n\n{\"content\":\"Funky!\"}\0");
            //setImmediate(sendMessage);
        }
    });
}

async function createSubscriptions(i) {
    for (let ix = 0; ix < i; ix++) {
        await request(`SUBSCRIBE\ndestination:/queue/${ix}\nid:1\n\0`);
    }

    return true;
}

function sendMessage() {
    conn.write("SEND\ndestination:/*\ncontent-type:text/plain\n\n{\"content\":\"Funky!\"}\0");
}