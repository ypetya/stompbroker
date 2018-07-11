const net = require('net');
const {log,error} = require('./logs');

const returnStatus = {connection: null, status:'NOT_CONNECTED'};

const connectDurable = port => {
    log('DC: Connecting...');
    const retry = () => setTimeout(() => connectDurable(port), 5000)
    connection = net.connect(port);
    connection.once('ready', ()=>{
        log('connected.');
        returnStatus.status = 'OK';
    });
    connection.once('error', (e) => {
        error(e.toString());
        returnStatus.status = 'ERROR';
    });
    connection.once('close', retry);

    return returnStatus;
}

module.exports = {
    connectDurable
}

