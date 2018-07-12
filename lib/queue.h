#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "general_list.h"

    typedef struct queue_st {
        general_list_item * first;
        general_list_item * last;
        int size;
    } queue;

    void enqueue(queue * q, void *new_data);
    void* dequeue(queue * q);
    
    void queue_free(queue *q);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */

