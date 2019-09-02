#!/bin/bash

# Compile and run unit tests
if gcc test.c -o test.out && ./test.out ; then
#if gcc test.c -o test.out -g && valgrind --leak-check=full --show-leak-kinds=all ./test.out ; then
 echo "PASS"
else
 echo "FAIL"
fi