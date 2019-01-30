#!/usr/bin/env node

const Stomp = require('stompjs');

const sock = Stomp.overWS('ws://localhost:3490');

//sock.debug = console.log;
const topic = `DIAG/${Math.random()}`;
console.log(Date.now(), topic);
sock.connect([], () => {
    sock.subscribe(topic, msg => {
        const input=msg.headers["input-queue-size"];
        const output=msg.headers["output-queue-size"];
        const stale=msg.headers["stale-queue-size"];
        const latency = Date.now() - Number(msg.headers.ts);
        console.log(`latency: ${latency}, input-queue-size: ${input}, output-queue-size:${output}, stale-queue-size:${stale}`);
    });
    setInterval(() =>
        sock.send(topic, { ts: Date.now() }, 'stale'), 500);
}, console.error);
