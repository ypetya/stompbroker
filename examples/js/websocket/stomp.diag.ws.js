#!/usr/bin/env node

const Stomp = require('stompjs');

const sock = Stomp.overWS('ws://localhost:3490');

//sock.debug = console.log;
const topic = `DIAG/${Math.random()}`;
console.log(Date.now(), topic);
sock.connect([], () => {
    sock.subscribe(topic, msg => {
        const {allocated,hits,misses,ts}=msg.headers;
        const latency = Date.now() - Number(ts);
        console.log(`latency: ${latency}, hits:${hits}, misses:${misses}, allocated:${allocated}`);
    });
    setInterval(() =>
        sock.send(topic, { ts: Date.now() }, 'ws_buffer'), 500);
}, console.error);
