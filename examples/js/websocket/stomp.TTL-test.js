#!/usr/bin/env node

const Stomp = require('stompjs');

const N = 1000;
const socks = [];
for (let j = 0; j < N; j++) {
  // create N websocket connections
  const sock = Stomp.overWS('ws://localhost:3490');
  socks.push(sock);
}

Promise.all(socks.map( (sock,index) => 
    new Promise(resolve => 
        sock.connect([], fr => {
            
            sock.send(`/topic/${index}`, 
            {}, `hello${index}`)

            sock.subscribe(`/topic/${index}`, msg => {
                                    sock.disconnect();
                                    resolve();
                                }, {id: index});
        }, e=>console.error(index,e) ))
    ))
    .then(() => console.log('OK'));