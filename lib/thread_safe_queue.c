
#include <stdio.h>
#include <stdlib.h>

#include "thread_safe_queue.h"

void ts_enqueue(ts_queue * q, void *new_data) {
    pthread_mutex_lock(&q->lock);
    enqueue(&q->q, new_data);
    pthread_mutex_unlock(&q->lock);
}

void ts_enqueue_multiple(ts_queue *q, general_list* new_items) {
    pthread_mutex_lock(&q->lock);
    general_list_item * first = new_items->first;
    while (first != NULL) {
        enqueue(&q->q, first->data);
        first = first->next;
    }
    pthread_mutex_unlock(&q->lock);
}

int ts_enqueue_limited(ts_queue * q, void *new_data, unsigned int limit) {
    int ret_val = 0;

    pthread_mutex_lock(&q->lock);
    if (q->q.size < limit)
        enqueue(&q->q, new_data);
    else
        ret_val = -1;
    pthread_mutex_unlock(&q->lock);

    return ret_val;
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
    pthread_mutex_init(&q->lock, (const pthread_mutexattr_t*) PTHREAD_PROCESS_PRIVATE);
}