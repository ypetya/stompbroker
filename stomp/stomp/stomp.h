#ifndef STOMP_H
#define STOMP_H

/**
 * # Responsibilities:
 * 
 * - process stomp messages
 * 
*/

#include "message.h"

void doStomp(ts_queue* output_queue, message *input);

#endif