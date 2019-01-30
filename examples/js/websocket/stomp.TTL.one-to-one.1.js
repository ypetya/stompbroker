#!/usr/bin/env node

/**
 * Description
 * ===========
 *
 * Create N connections.
 * send one message on each.
 * subscribe later, and disconnect if message received each.
 *
 */

const Stomp = require('stompjs');

const N = 800;
const socks = [];
for (let j = 0; j < N; j++) {
  // create N websocket connections
  const sock = Stomp.overWS('ws://localhost:3490');
  //   sock.debug = console.log;
  socks.push(sock);
}

const MAP = {};

Promise
    .all(
        socks.map(
            (sock, index) => 
            new Promise( resolve => sock.connect(
                    [],
                    fr => {
                      // console.log(`+${index} session:${fr.headers.session}`);
                      MAP[index] = fr.headers.session;
                      sock.send(`/topic/${index}`, {}, `hello${index}`);
                      resolve(sock);
                    },
                    console.log))))
    .then(
        socks => Promise.all(
            socks.map((sock, index) => new Promise(resolve => {
                sock.subscribe(
                    `/topic/${index}`, msg => {
                    // console.log(`-${index}.`);
                    sock.disconnect();
                    delete (MAP[index]);
                    resolve();
                    }, {id: index});
            }))))
    .then(() => console.log('OK'));


setInterval(() => {
  Object.keys(MAP).forEach(k => console.log(`Missing ${k}, fd:${MAP[k]}`));
}, 10000);