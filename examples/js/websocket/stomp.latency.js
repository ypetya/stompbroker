#!/usr/bin/env node

const Stomp = require('stompjs');

const sock = Stomp.overWS(process.env.WS || 'ws://localhost:3490');

//sock.debug = console.log;
const topic = `/latency/${Math.random()}`;
console.log(Date.now(), topic);
sock.connect([], () => {
    sock.subscribe(topic, msg => {
        console.log(Date.now() - Number(msg.headers.ts));
    });
    setInterval(() =>
        sock.send(topic, { ts: Date.now() }, 'hello'), 500);
}, console.error);
