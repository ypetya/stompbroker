#ifndef OUTPUT_QUEUE_H
#define OUTPUT_QUEUE_H

#include "../../lib/thread_safe_queue.h"
#include "../data/message/with_payload_length.h"
#include "output_batch.h"

#define MESSAGES_BATCH_SIZE 1000

/**
 * Takes the next batch of output messages for one connection.
 *
 * Blocks until a batch is available. Returns the connection's real fd (flags
 * masked off); the caller owns that fd until output_queue_release_fd().
 * Returns -1 if the batch is a poison pill.
 */
int output_queue_take_batch(message_with_frame_len *(*ret)[MESSAGES_BATCH_SIZE], ts_queue *q, int buffer_size);

/** Hands the fd back after its batch is written (or dropped). */
void output_queue_release_fd(ts_queue *q, int fd);

/**
 * Takes the fd like a writer would, waiting until no writer holds a batch of
 * it. Used by the close path: closing while owning the fd guarantees that no
 * writer is about to write old bytes into a reused fd number.
 * Release with output_queue_release_fd().
 */
void output_queue_claim_fd(ts_queue *q, int fd);

/**
 * Slow consumer budget of a connection. Only the current owner of the fd
 * (see above) may read or change it.
 */
output_stall *output_queue_stall_of(int fd);

#endif /* OUTPUT_QUEUE_H */
