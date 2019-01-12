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

    // TODO: should be a list action instead
    void unchain_child(queue * q, general_list_item * parent, general_list_item * child);
    
    void queue_free(queue *q);
    void queue_init(queue *q);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */

