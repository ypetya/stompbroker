#!/usr/bin/env node

const Stomp = require('stompjs');

const N = 3000;
const socks = [];
for (let j = 0; j < N; j++) {
  // create N websocket connections
  const sock = Stomp.overWS('ws://localhost:3490');
  socks.push(sock);
}

Promise.all(socks.map( (sock,index) => 
    new Promise(resolve => 
        sock.connect([], fr => {
            //console.log(`+${index}`);
            sock.send(`/topic/${index}`, {}, `hello${index}`);

            setTimeout( ()=>
                sock.subscribe(`/topic/${index}`, msg => {
              //                      console.log(`-${index}.`);
                                    sock.disconnect();
                                    resolve();
                                }, {id: index}), 500); // => ensure running with TTL >
       
        }, e=>console.error(index,e) ))
    ))
    .then(() => console.log('OK'));