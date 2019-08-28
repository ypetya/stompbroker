#!/bin/bash

# Compile and run unit tests
if gcc test.c -o test.out && ./test.out ; then
 echo "PASS"
else
 echo "FAIL"
fi