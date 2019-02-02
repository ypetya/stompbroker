#!/usr/bin/env node

const Stomp = require('stompjs');

const wsUrl = 'ws://localhost:3490';
const topic = `DIAG/${Math.random()}`;
console.log(Date.now(), topic);

let sock = null;
let connected = false;

setInterval(() => {
        if(connected) 
            sock.send(topic, { ts: Date.now() }, 'stale')
}, 500);

const connect = () => {
    sock = Stomp.overWS(wsUrl);
    //sock.debug = console.log;
        
    sock.connect([], () => {
        sock.subscribe(topic, msg => {
            const input=msg.headers["input-queue-size"];
            const output=msg.headers["output-queue-size"];
            const stale=msg.headers["stale-queue-size"];
            const latency = Date.now() - Number(msg.headers.ts);
            console.log(`latency: ${latency}, input-queue-size: ${input}, output-queue-size:${output}, stale-queue-size:${stale}`);
        });
        connected=true;
        console.log(`Connected to ${wsUrl}`);
    }, error => {
        connected=false;
        console.error(error);
    });
}

setInterval(()=>{
    if(!connected){
        console.log('Connecting ...');
        connect();
    }
}, 1000);