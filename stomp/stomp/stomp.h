#ifndef STOMP_H
#define STOMP_H

/**
 * # Responsibilities:
 * 
 * - process stomp messages
 * 
*/

#include "message.h"
#include "../../lib/thread_safe_queue.h"
void stomp_start();
void stomp_process(ts_queue* output_queue, message *input);
void stomp_stop();
#endif