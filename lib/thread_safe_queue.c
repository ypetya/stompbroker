

#include <stdio.h>
#include <stdlib.h>

#include "thread_safe_queue.h"

void ts_enqueue(ts_queue * q, void *new_data) {
    pthread_mutex_lock(&q->lock);
    enqueue(&q->q, new_data);
    pthread_mutex_unlock(&q->lock);
}

void* ts_dequeue(ts_queue * q) {
    pthread_mutex_lock(&q->lock);
    void * retVal = dequeue(&q->q);
    pthread_mutex_unlock(&q->lock);
    return retVal;
}

// NOTE: caution! - frees up the data!

void ts_queue_free(ts_queue *q) {
    pthread_mutex_lock(&q->lock);
    queue_free(&q->q);
    pthread_mutex_unlock(&q->lock);
}

void ts_queue_init(ts_queue *q) {
    // NULL -> default behaviour
    queue_init(&q->q);
    pthread_mutex_init(&q->lock,(const pthread_mutexattr_t*)PTHREAD_PROCESS_PRIVATE);
}