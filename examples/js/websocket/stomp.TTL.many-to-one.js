#!/usr/bin/env node

/**
 * Test description
 * ================
 * 
 * Create one websocket connection.
 * Send N messages to a topic.
 * Create one subscription, 
 * wait all the messages to arrive.
 */

const Stomp = require('stompjs');

const N = 1000;
const sock = Stomp.overWS('ws://localhost:3490');

let i = 0;

const p = new Promise(resolve => sock.connect([], resolve));

p.then(resolve => {
  for (let i = 0; i < N; i++) sock.send(`/topic/1`, {}, `hello${i}`);
  resolve(N);
})
.then( n=> console.log(`${n} messages sent.`))
.then(()=> sock.disconnect())
.then(()=> new Promise(resolve=>setTimeout(resolve,2000)))
.then(()=> new Promise(resolve => sock.connect([], resolve)))
.then(()=> new Promise(resolve => sock.subscribe('/topic/1',msg=>{
  if (++i >= N) resolve(); })))
.then(()=>console.log('All message arrived!'))
.catch(e => console.error('An error happened:',e));
