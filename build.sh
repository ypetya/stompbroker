#!/bin/bash

rm -v *.out
DEBUG_OPTS=""
if [ "debug" == "$1" ]; then
    DEBUG_OPTS="-g"
    echo "compiling with debug option."
fi
if [ "valgrind" == "$1" ]; then
    DEBUG_OPTS="-g"
fi
if gcc -std=gnu11 -pthread main.c -lpthread -o stompbroker.out $DEBUG_OPTS
then 
    echo 'Build done!'
else
    echo 'Build failed! :('
fi
if [ "valgrind" == "$1" ]; then
 valgrind --leak-check=full --show-leak-kinds=all ./stompbroker.out TTL=1000000
fi
