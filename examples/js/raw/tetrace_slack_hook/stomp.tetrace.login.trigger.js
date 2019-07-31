#!/usr/bin/env node

/**
 * Hook into the tetrace login messages -> call slack url
 * */

const net = require('net');
const connArr = [];

['tetrace_ws_host', 'slack_hook_url'].forEach(p => {
    if (!process.env[p]) {
        console.log(`Environment variable "${p}" not set!`);
        exit(1);
    }
});


const conn = net.connect(3490, 'localhost');
conn.setNoDelay(true);
conn.once('ready', onReady.bind(conn, i));
conn.once('error', onError.bind(conn, i));
conn.once('close', onClose.bind(conn, i));
conn.on('data', onData.bind(conn, i));

connArr.push(conn);
}

console.log(`Created ${connArr.length} connections.`);

const stompArr = [];

function onReady(i) {
    console.log(`Sending connect frame:${i}`);
    stompArr[i] = 'CONNECTING';
    this.write("STOMP\naccept-version:1.2\nhost:localhost\n\n");
    this.once('data', d => {
        const data = d.toString();
        if (data.indexOf('CONNECTED') == 0) {
            stompArr[i] = 'CONNECTED';
            var matches = data.match(/session:(\d+)/);
            if (matches.length > 0) {
                const sessionId = matches[1];
                let stillConnecting = 0;
                stompArr.forEach(s => {
                    if (s == 'CONNECTING')
                        stillConnecting++;
                });
                console.log(`connected. session_id: ${sessionId}. Left:${stillConnecting}`);
                this.write(`SUBSCRIBE\ndestination:/queue/*\nid:${i}\n`);
            }
        }
    }
    );
}

function onError(i, e) {
    console.error(`${i}: ${e.toString()}`);
}

function onClose(i) {
    console.log(`Connection closed: ${i}`);
}

let arrived = 0;
function onData(conn, i, d) {
    //arrived++;
    /*${d.toString()}\n*/
    //console.log(`Arrived ${arrived++}\n`);
}
