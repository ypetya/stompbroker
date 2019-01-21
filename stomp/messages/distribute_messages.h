#ifndef DISTRIBUTE_MESSAGES_H
#define DISTRIBUTE_MESSAGES_H

#ifdef __cplusplus
extern "C" {
#endif


#define STOMP_MESSAGE_CONSUMED 1
#define STOMP_MESSAGE_NOT_CONSUMED 0

#include "../../server/data/message/with_timestamp.h"
#include "../parser.h"
#include "../../lib/thread_safe_queue.h"

int distribute_messages(ts_queue* input_queue, 
    ts_queue* output_queue, 
    message_with_timestamp *input, 
    parsed_message * pm,
    unsigned int ttl);

#ifdef __cplusplus
}
#endif

#endif /* DISTRIBUTE_MESSAGES_H */

