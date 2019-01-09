#!/usr/bin/env node

const Stomp = require('stompjs');

const sock = Stomp.overWS('ws://localhost:3490');

//sock.debug = console.log;

sock.connect([],()=>{
    sock.subscribe('/queue/*', console.log);
    sock.send('/queue/1',[],'hello');
}, console.error);