#ifndef PROCESS_WRITER_THREAD_H
#define PROCESS_WRITER_THREAD_H

#include "output_queue.h"
#include "output_batch.h"

void *writer_thread(void *vargp);

/**
 * Writes one batch taken with output_queue_take_batch(). Messages of a closed
 * session (fd not in use, or another generation) are dropped. A write error,
 * slow consumer or a close_after message requests closing the connection
 * (shutdown; the epoll thread closes the fd). Frees the messages.
 * @return 1 when the batch was a poison pill (the writer must exit), 0 otherwise
 */
int writer_send_batch(message_with_frame_len *(*messages)[MESSAGES_BATCH_SIZE], output_batch *batch);

#endif
