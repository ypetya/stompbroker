#!/usr/bin/env node

const TEST_DATA = [
    "STOMP\naccept-version:1.2\nhost:localhost\n\n", "CONNECTED\nversion:1.2\n\n",
    "CONNECT\naccept-version:1.2\nhost:localhost\n\n", "CONNECTED\nversion:1.2\n\n",
    "CONNECT\naccept-version:1.1\nhost:localhost\n\n", "CONNECTED\nversion:1.2\n\n",
    "CONNECT\r\naccept-version:1.1\nhost:localhost\n\n", "CONNECTED\nversion:1.2\n\n",
    "CONNECT\naccept-version:1.1\nhost:localhost\nbody\n", "ERROR\ncontent-length:15\ncontent-type:text;charser=utf-8\n\ninvalid message"
];

let {connectDurable} = require('./connectDurable')
let {runChecksAndRepeat} = require('./requestUtils');

console.log('Started');
let conn = connectDurable(3490);
runChecksAndRepeat(conn)(TEST_DATA);
