
# Library

__TOC__

## queue

A basic queue data structure with O(1) enqueue and dequeue.
Maintains the size information

## thread_safe_queue

Inserting and removing elements are guarded by pthread_mutex locks.
Based on queue

## avltree

Not implemented yet

there is a need to implement key-value based data structure.
avltree is a balanced tree focusing on the case where the get/insert
ratio is high.

## associative_array

A basic tree structure with a node having a left, right element.
Each node can contain a key and a value char *.

## clone_str

Cloning a string based on emalloc, which clears the allocated memory with zeroes 
('\0')

## logger

Basic logger macros, implementing levels

* print
* warn
* info 
* debug
* trace

## general_list

A basic one-way chained list. 
The list consist a pointer to the first and the last element.
Inserting is O(1).
Each element has a pointer to the next element.

## random_string

print_random_str of a `len` length based on an alphabet

## string_list

a basic one-way chained list of strings.
