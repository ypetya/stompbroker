#!/bin/bash

killall -v stompbroker.out
pkill -ef 'node ./test/stomp.test.js'

rm -v *.out

if gcc main.c -o stompbroker.out -lpthread $DEBUG_OPTS
then 
    echo 'Build done!'
else
    echo 'Build failed! :('
fi

./stompbroker.out &
./test/stomp.test.js &