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
void stomp_process(ts_queue* input_queue, queue* stale_queue, ts_queue* output_queue, message_with_timestamp *input);
void stomp_stop(queue * stale_queue);
#endif