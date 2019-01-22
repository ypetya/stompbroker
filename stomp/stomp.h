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
int stomp_process(ts_queue* input_queue, ts_queue* output_queue, message_with_timestamp *input);
void stomp_stop();
#endif