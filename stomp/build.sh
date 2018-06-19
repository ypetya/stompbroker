#!/bin/bash

rm -v *.out
DEBUG_OPTS=""
if [ "debug" == "$1" ]; then
    DEBUG_OPTS="-g"
    echo "compiling with debug option."
fi
if gcc main.c -o stompbroker.out -lpthread $DEBUG_OPTS
then 
    echo 'Build done!'
else
    echo 'Build failed! :('
fi
