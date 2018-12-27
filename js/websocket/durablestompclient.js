const WebSocket = require('ws');
const Stomp = require('stompjs');

module.exports = class DurableStompClient {
    constructor({ retryCount = 100, debug = console.log, url = '/ws', headers = {},
        onConnect = data => console.log('StompConnect', data) ,
        onError = error => console.error('StompError', error)}) {
        this.retryCount = retryCount;
        this.debug = debug;
        this.url = url;
        this.stompHeaders = headers;
        this.stompConnectCallback = onConnect;
        this.stompErrorCallback = onError;
        this.connectSocket();
    }

    connectSocket() {
        this.retryCount -= 1;
        this.errorTimer = null;
        
        this.socket = new WebSocket(this.url);

        this.socket.on('open', () => {
            this.debug('Websocket onOpen!');
            this.socketState = 'OPEN';
        });
        this.socket.on('close', () => {
            this.debug('Websocket onClosed!');
            this.socketState = 'CLOSED';
            this.stompState = 'CLOSED';
            this.reconnect();
        });

        this.connectStomp();
    }

    connectStomp() {
        this.stompClient = Stomp.over(this.socket);
	    this.stompClient.debug = this.debug;
        this.send = this.stompClient.send.bind(this.stompClient);
        this.subscribe = this.stompClient.subscribe.bind(this.stompClient);
        
        this.stompClient.connect(this.stompHeaders, this.stompConnectCallback, error => {
            this.reconnect();
            this.stompErrorCallback(error);
        });
    }

    reconnect() {
        if (!this.errorTimer) {
            if (this.retryCount > 0) {
                this.errorTimer = setTimeout(this.connectSocket.bind(this), 100);
            } else {
                console.error('Maximum retry count reached! Gave up reconnecting stomp.');
            }
        }
    }
}
