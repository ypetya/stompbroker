#!/bin/bash

if [ "$(uname -s)" != "Linux" ]; then
    echo "The broker uses epoll and builds on Linux only."
    echo "On $(uname -s) run the portable unit tests (make test test-ws) or everything in a container (make docker-test)."
    exit 1
fi

rm -fv *.out
DEBUG_OPTS=""
if [ "debug" == "$1" ]; then
    DEBUG_OPTS="-g"
    echo "compiling with debug option."
fi
if [ "valgrind" == "$1" ]; then
    DEBUG_OPTS="-g"
fi
if gcc -std=gnu11 -pthread main.c -lpthread -o stompbroker.out -Wall $DEBUG_OPTS
then 
    echo 'Build done!'
else
    echo 'Build failed! :('
    exit 1
fi
if [ "valgrind" == "$1" ]; then
 valgrind --leak-check=full --show-leak-kinds=all ./stompbroker.out TTL=1000000
fi
