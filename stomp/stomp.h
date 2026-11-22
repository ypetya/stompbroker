#ifndef STOMP_H
#define STOMP_H

/**
 * # Responsibilities:
 * 
 * - process stomp messages
 * 
 */

#include "../server/data/message/with_timestamp.h"
#include "../lib/thread_safe_queue.h"
void stomp_start();
int stomp_process(ts_queue* input_queue, queue* stale_queue, ts_queue* output_queue, message_with_timestamp *input);
void stomp_stop(queue * stale_queue);

/** Is ack_timeout configured? Then stomp_redeliver_timed_out has to be called periodically. */
int stomp_ack_timeout_enabled(void);
/** Redelivers the worker queue messages not acknowledged within ack_timeout. */
void stomp_redeliver_timed_out(queue * stale_queue, ts_queue * output_queue);
#endif