#!/bin/bash

# Compile and run unit tests
if make test test-ws ; then
#if gcc test.c -o test.out -g && valgrind --leak-check=full --show-leak-kinds=all ./test.out ; then
 echo "PASS"
else
 echo "FAIL"
fi