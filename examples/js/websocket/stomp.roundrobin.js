#!/usr/bin/env node

const Stomp = require('stompjs');
const { fork } = require('child_process');
const URL = process.env.WS || 'ws://localhost:3490';
const topic = `#${Math.random()}`;

console.log('Roundrobin started', Date.now(), topic);

const NUMBER_OF_CHILDREN = 10;
let connected = 0;

const everyBodyConnected = new Promise( 
    (resolve) => {
        for(let i=0;i<NUMBER_OF_CHILDREN ;i++){
               const process = fork('./stomp.roundrobin.listener.js');

               process.on('message', ({id, status}) => {
                   console.log(['Child',id,'reported',status].join(' '));
                   if(++connected === NUMBER_OF_CHILDREN) resolve('go! :)');
               });

               process.send({id:i, topic, url:URL});
           }
        });

everyBodyConnected.then( (message)=> {

const sock = Stomp.overWS(URL);

sock.connect([], () => {
    setInterval(() =>
        sock.send(topic, { ts: Date.now() }, message), 500);
}, console.error);

});   

