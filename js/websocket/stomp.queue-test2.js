#!/usr/bin/env node

const Stomp = require('stompjs');

const sock = Stomp.overWS('ws://localhost:3490');
//sock.debug = console.log;

let sum = 0;

sock.connect([], () => {

    sock.subscribe('/queue/*', msg => sum++, { id: 'sub-1' });
    sock.subscribe('/queue/*', msg => sum--, { id: 'sub-2' });
    let total=0;
    setInterval(() => {
        for (let i = 0; i < 1000; i++) {
            sock.send(`/queue/${i}`, [], `hello ${++total}`);
        }
    }, 10000);
    setInterval(() => console.log(sum), 2000);
}, console.error);