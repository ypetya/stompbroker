#!/bin/sh
# 
# File:   test.debug.sh.sh
# Author: peter
#
# Created on Sep 8, 2019, 5:19:21 PM
#
gcc -std=gnu11 -pthread test.c -lpthread -o test.out -Wall -g