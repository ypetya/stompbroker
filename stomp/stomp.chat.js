#!/usr/bin/env node


const net = require('net');

const conn = net.connect(3490)
conn.setNoDelay(true);
conn.once('ready', () => {
    conn.write("STOMP\naccept-version:1.2\nhost:localhost\n\n");
    conn.once('data', d => {
        if (d.toString().indexOf('CONNECTED') == 0) {
            console.log('connected.');
            runChat();
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
