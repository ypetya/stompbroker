#!/bin/bash

#killall -v stompbroker.out
#pkill -ef 'node ./test/stomp.test.js'

#rm -v *.out

if gcc main.c -o stompbroker.out -lpthread $DEBUG_OPTS
then 
    echo 'Build done!'
else
    echo 'Build failed! :('
fi

if ! ulimit -n 5000 
then 
    echo 'ulimit: open file descriptors count too large!'
fi
./stompbroker.out
