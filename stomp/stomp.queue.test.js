#!/usr/bin/env node

const net = require('net');

const conn = net.connect(3490)
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

function runTest() {
    createSubscriptions(10000).then(
            ()=> setImmediate(sendMessage));
}

const request = m => new Promise((resolve, reject) => {
        setImmediate(()=>conn.write(m));
        setTimeout(resolve,0);
    });

async function createSubscriptions(i) {
    for (let ix = 0; ix < i; ix++) {
        await request(`SUBSCRIBE\ndestination:/queue/${ix}\nid:1\n`);
    }
    
    return true;
}

function sendMessage() {
    conn.write("SEND\ndestination:/queue/*\ncontent-type:text/plain\n\nFunky!\n");
}