#!/usr/bin/env node

const Stomp = require('stompjs');

const sock = Stomp.overWS('ws://localhost:3490');
//sock.debug = console.log;

let sum = 0;

sock.connect([], () => {

    sock.subscribe('/queue/*', msg => sum++, { id: 'sub-1' });
    sock.subscribe('/queue/*', msg => sum--, { id: 'sub-2' });

    for(let i=0;i<20000;i++){
        sock.send(`/queue/${i}`, [], 'hello');
    }
    setTimeout(()=>console.log(sum),5000);
}, console.error);