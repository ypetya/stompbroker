#!/usr/bin/env node
const Client = require('./durablestompclient');

const c = new Client({
    url:'http://localhost:3490',
    onConnect: ()=> {
        c.subscribe('/*', console.log);
    }
})