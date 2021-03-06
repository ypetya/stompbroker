#!/usr/bin/env node

const Stomp = require('stompjs');

const sock = Stomp.overWS('ws://localhost:3490');
//sock.debug = console.log;

//const subs_count = 100, display_interval = 1000, send_interval = 1000, send_amount = 1500;
// limit
//const subs_count = 1750, display_interval = 1000, send_interval = 800, send_amount = 100;
const subs_count = 1000, display_interval = 1000, send_interval = 500, send_amount = 10000;

// -> Total messages sent: 600000, received 1188229 on 2 subscriptions. (#60)
//subs_count = 75, display_interval = 1000, send_interval = 1000, send_amount = 2000
let total_received = 0, total_sent = 0;
let display_intervals_count = 0;
sock.connect([], () => {
    for (let i = 0; i < subs_count; i++) {
        sock.subscribe('/queue/*', msg => total_received++, { id: `sub-${i}` });
    }

    setInterval(() => {
        try {
            for (let i = 0; i < send_amount; i++) {
                sock.send(`/queue/${i}`, [], `hello ${++total_sent}`);
            }
        } catch (e) {
            console.error(e);
        }
    }, send_interval);

    setInterval(() => console.log(`Total messages sent: ${total_sent}, received ${total_received} on ${subs_count} subscriptions. (#${++display_intervals_count})`), display_interval);
}, e=>{
    console.error(e);
    console.trace();
    process.exit(1);
});
