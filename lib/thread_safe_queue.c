
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "thread_safe_queue.h"

void ts_enqueue(ts_queue * q, void *new_data) {
    pthread_mutex_lock(&q->lock);
    enqueue(&q->q, new_data);
    pthread_cond_signal(&q->has_new_elements);
    pthread_mutex_unlock(&q->lock);
}

void ts_put_head(ts_queue *q, void *new_data) {
    pthread_mutex_lock(&q->lock);
    put_head(&q->q, new_data);
    pthread_cond_signal(&q->has_new_elements);
    pthread_mutex_unlock(&q->lock);
}

void ts_enqueue_multiple(ts_queue *q, general_list* new_items) {
    pthread_mutex_lock(&q->lock);
    general_list_item * first = new_items->first;
    if (first) {
        pthread_cond_signal(&q->has_new_elements);
        do {
            enqueue(&q->q, first->data);
            first = first->next;
        } while (first);
    }
    pthread_mutex_unlock(&q->lock);
}

int ts_enqueue_limited(ts_queue * q, void *new_data, unsigned int limit) {
    int ret_val = 0;

    pthread_mutex_lock(&q->lock);

    if (q->q.size < limit) {
        enqueue(&q->q, new_data);
        pthread_cond_signal(&q->has_new_elements);
    } else
        ret_val = -1;

    pthread_mutex_unlock(&q->lock);

    return ret_val;
}

void* ts_dequeue(ts_queue * q) {
    pthread_mutex_lock(&q->lock);
    while (q->q.size == 0) pthread_cond_wait(&q->has_new_elements, &q->lock);
    void * retVal = dequeue(&q->q);
    pthread_mutex_unlock(&q->lock);
    return retVal;
}

// NOTE: caution! - frees up the data!

void ts_queue_free(ts_queue *q) {
    pthread_mutex_lock(&q->lock);
    queue_free(&q->q);
    pthread_mutex_unlock(&q->lock);
    // free up cond
    pthread_cond_destroy(&q->has_new_elements);

}

void ts_queue_init(ts_queue *q) {
    queue_init(&q->q);
    // PROCESS private condition and mutex
    pthread_cond_init(&q->has_new_elements, NULL);
    pthread_mutex_init(&q->lock, (const pthread_mutexattr_t*) PTHREAD_PROCESS_PRIVATE);
}