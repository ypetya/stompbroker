#!/usr/bin/env node

const Stomp = require('stompjs');

const sock = Stomp.overWS('ws://localhost:3490');

//sock.debug = console.log;
const topic = `DIAG/${Math.random()}`;
console.log(Date.now(), topic);
sock.connect([], () => {
    sock.subscribe(topic, msg => {
        const {clients,websockets,stomp_connected,subscriptions,ts}=msg.headers;
        const latency = Date.now() - Number(ts);
        console.log(`latency: ${latency}, clients:${clients}, websockets:${websockets}, stomp_connected:${stomp_connected}, subscriptions:${subscriptions}`);
    });
    setInterval(() =>
        sock.send(topic, { ts: Date.now() }, 'session'), 500);
}, console.error);
