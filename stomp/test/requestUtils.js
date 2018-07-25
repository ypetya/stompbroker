const {log,error} = require('./logs');

const request = connection => m => new Promise((resolve, reject) => {
    connection.once('data', resolve)
    connection.write(m);
    setTimeout(() => reject('timeout'), 100);
});

const awaitGoodResponse = configuredRequest => async (requestMessage, responseMessage) => {
    try {
        const response = await configuredRequest(requestMessage);

        if (response != responseMessage) {
            error(`ERR: Invalid response:\nExpected to get:${responseMessage}\nGot:${response}\n\n`);
        }
    } catch (err) {
        error(`ERR: ${err}\n${requestMessage}\n\n`);
    }
};

const runChecksAndRepeat = conn => testData => {
    try {
        let lastPromise = Promise.resolve(1);
        let {connection,status} = conn,
            configuredRequest = request(connection);

        if(status==='OK'){
            log('Running tests...');
            for (let cursor = 0; cursor < testData.length; cursor += 2) {
                lastPromise = awaitGoodResponse(configuredRequest)(testData[cursor], testData[cursor + 1])
                    .then(() => console.log(`SUCC: ${cursor}`));
            }
        } else log(conn);

        lastPromise.then(() => {
            if(status==='OK'){
                log('Done. Restarting in 10sec');
            }
            setTimeout(()=>runChecksAndRepeat(conn)(testData), 10000);
        });
    }
    catch (err) {
        log('Error at test execution, retrying in 10 secs....');
        setTimeout(()=>runChecksAndRepeat(conn)(testData), 10000);
    }
}

module.exports = {
    runChecksAndRepeat
}