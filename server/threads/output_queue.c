#include "output_queue.h"
#include <stdlib.h>
#include <pthread.h>
#include "../data/session_storage.h"

/*
 * Ordering rules for the output queue:
 *
 * - Messages are grouped by the real fd (flags masked off). Raw output
 *   (HTTP 101, pong, close frames) is queued with the bare fd while STOMP
 *   replies of the same WebSocket connection carry the encoded flag; both
 *   must stay in one FIFO stream.
 * - At most one writer owns an fd at a time. A writer that is still sending
 *   a batch for fd N keeps every later message for N in the queue, so two
 *   writers never race to send batches of one connection in the wrong order.
 *
 * fd_owned is guarded by the queue lock.
 */
static unsigned char fd_owned[FD_MASK + 1];

static int is_poison_pill(const message_with_frame_len *msg) {
    return msg->fd == -1;
}

static void take_item(ts_queue *q, general_list_item *parent, general_list_item *item,
        message_with_frame_len *(*ret)[MESSAGES_BATCH_SIZE], int *index) {
    (*ret)[(*index)++] = item->data;
    unchain_child(&q->q, parent, item);
    free(item);
}

/** @return the claimed real fd, -1 for a poison pill, 0 if nothing is eligible yet */
static int take_batch_locked(message_with_frame_len *(*ret)[MESSAGES_BATCH_SIZE], ts_queue *q, int buffer_size) {
    general_list_item *parent = NULL;
    general_list_item *cursor = q->q.first;
    int claimed_fd = 0;
    int index = 0;
    long remaining_len = buffer_size;

    while (index < MESSAGES_BATCH_SIZE && cursor != NULL && remaining_len > 0) {
        general_list_item *next = cursor->next;
        message_with_frame_len *msg = cursor->data;

        if (is_poison_pill(msg)) {
            if (index > 0) break; // finish the current batch first
            take_item(q, parent, cursor, ret, &index);
            return -1;
        }

        int fd = session_without_flags(msg->fd);
        int eligible = claimed_fd == 0 ? !fd_owned[fd] : fd == claimed_fd;
        if (!eligible) {
            parent = cursor; // other / busy connection: leave it in place
        } else {
            if (claimed_fd != 0 && (long) msg->frame_len > remaining_len) break; // keep FIFO: stop here
            claimed_fd = fd;
            remaining_len -= (long) msg->frame_len;
            take_item(q, parent, cursor, ret, &index); // parent stays the same
        }
        cursor = next;
    }

    if (claimed_fd != 0) fd_owned[claimed_fd] = 1;
    return claimed_fd;
}

int output_queue_take_batch(message_with_frame_len *(*ret)[MESSAGES_BATCH_SIZE], ts_queue *q, int buffer_size) {
    int fd = 0;
    pthread_mutex_lock(&q->lock);
    // Wait while the queue is empty or holds only output of connections
    // other writers own; enqueue and release both signal the condition.
    while (q->q.size == 0 || (fd = take_batch_locked(ret, q, buffer_size)) == 0)
        pthread_cond_wait(&q->has_new_elements, &q->lock);
    pthread_mutex_unlock(&q->lock);
    return fd;
}

void output_queue_release_fd(ts_queue *q, int fd) {
    pthread_mutex_lock(&q->lock);
    fd_owned[session_without_flags(fd)] = 0;
    pthread_cond_broadcast(&q->has_new_elements);
    pthread_mutex_unlock(&q->lock);
}
