
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
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

static long long monotonic_ns(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long long) now.tv_sec * 1000000000LL + now.tv_nsec;
}

static struct timespec timespec_of_ns(long long ns) {
    struct timespec t = {(time_t) (ns / 1000000000LL), (long) (ns % 1000000000LL)};
    return t;
}

/*
 * Waits on has_new_elements until deadline_ns (CLOCK_MONOTONIC), so stepping
 * the wall clock does not stretch or cut the wait. Linux: the condvar uses
 * CLOCK_MONOTONIC (see ts_queue_init). macOS has no pthread_condattr_setclock:
 * it waits for the remaining time, a relative wait that ignores the wall clock.
 * @return 0 if woken up, non-zero on time-out or error
 */
static int wait_until(ts_queue * q, long long deadline_ns) {
#if defined(__APPLE__)
    long long remaining = deadline_ns - monotonic_ns();
    if (remaining <= 0) return ETIMEDOUT;
    struct timespec relative = timespec_of_ns(remaining);
    return pthread_cond_timedwait_relative_np(&q->has_new_elements, &q->lock, &relative);
#else
    struct timespec deadline = timespec_of_ns(deadline_ns);
    return pthread_cond_timedwait(&q->has_new_elements, &q->lock, &deadline);
#endif
}

void* ts_dequeue_timed(ts_queue * q, long timeout_ms) {
    long long deadline_ns = monotonic_ns() + (long long) timeout_ms * 1000000LL;
    pthread_mutex_lock(&q->lock);
    // any non-zero result (time-out, error) ends the wait: never spin on EINVAL
    while (q->q.size == 0 && wait_until(q, deadline_ns) == 0);
    void * retVal = dequeue(&q->q); // NULL if still empty
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
    pthread_condattr_t attributes;
    pthread_condattr_init(&attributes);
#if !defined(__APPLE__)
    pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC); // ts_dequeue_timed deadlines
#endif
    pthread_cond_init(&q->has_new_elements, &attributes);
    pthread_condattr_destroy(&attributes);
    pthread_mutex_init(&q->lock, NULL); // default attributes are process-private
}