#ifndef TS_QUEUE_H
#define TS_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include "queue.h"

    typedef struct queue_st {
        queue q;
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    } ts_queue;

    void ts_enqueue(ts_queue * q, void *new_data);
    void* ts_dequeue(ts_queue * q);
    
    void ts_queue_free(ts_queue *q);

#ifdef __cplusplus
}
#endif

#endif /* TS_QUEUE_H */

