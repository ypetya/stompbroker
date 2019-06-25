#!/usr/bin/env node

const Stomp = require('stompjs');

process.on('message', ({id, url, topic})=>{

    console.log(['Child process',id, 'connecting', url, topic].join(' '));
    
    try{

        const sock = Stomp.overWS(url);

        sock.connect([], () => {
            sock.subscribe(topic, msg => {
                console.log(['Child', id, 'Latency:', Date.now() - Number(msg.headers.ts), 'Message:', msg.body].join(' '));
            });
            process.send({id, status:'OK'});
        }, console.error);

    } catch(err) {
    
        process.send({id, status: err});
    }
});
