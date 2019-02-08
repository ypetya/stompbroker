#ifndef TS_QUEUE_H
#define TS_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include "queue.h"
#include "general_list.h"

    typedef struct ts_queue_st {
        queue q;
        pthread_mutex_t lock;
    } ts_queue;

    void ts_queue_init(ts_queue *q);
    void ts_enqueue(ts_queue * q, void *new_data);
    void ts_put_head(ts_queue * q, void *new_data);
    void ts_enqueue_multiple(ts_queue *q, general_list* new_items);
    int ts_enqueue_limited(ts_queue * q, void *new_data, unsigned int limit);
    void* ts_dequeue(ts_queue * q);
    
    void ts_queue_free(ts_queue *q);

#ifdef __cplusplus
}
#endif

#endif /* TS_QUEUE_H */

