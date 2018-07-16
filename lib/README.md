
# Library

These structures got into a more mature state


## thread_safe_queue

inserting and removing elements are guarder by pthread_mutex locks

## avltree

there is a need to implement key-value based data structure.
avltree is a balanced tree focusing on the case where the get/insert
ratio is high.