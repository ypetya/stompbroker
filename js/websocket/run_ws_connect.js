#!/usr/bin/env node
const Client = require('./durablestompclient');

const c = new Client({
    url:'ws://localhost:3490',
    onConnect: ()=> {
        c.subscribe('/*', console.log);
    }
})