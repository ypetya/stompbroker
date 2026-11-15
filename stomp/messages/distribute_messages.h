#ifndef DISTRIBUTE_MESSAGES_H
#define DISTRIBUTE_MESSAGES_H

#ifdef __cplusplus
extern "C" {
#endif


#define STOMP_MESSAGE_CONSUMED 1
#define STOMP_MESSAGE_NOT_CONSUMED 0

#include "../../server/data/message/with_timestamp.h"
#include "../parser.h"
#include "../../lib/queue.h"
#include "../../lib/thread_safe_queue.h"

int distribute_messages(
    ts_queue* input_queue,
    unsigned max_stale_queue_size,
    queue * stale_queue,
    ts_queue* output_queue, 
    message_with_timestamp *input, 
    parsed_message * pm,
    unsigned int ttl);

/** Bytes the stale queue is charged with (bounded by stale_queue_max_bytes). */
size_t stale_queue_bytes(void);
extern size_t stale_queue_max_bytes;

/** Frees every stored message. */
void stale_queue_clear(queue * stale_queue);

void distribute_messages_from_stale_q(
    char * topic, int client_session_id, char* subscription_id, unsigned int generation,
    queue * stale_queue,
    ts_queue* output_queue,
    unsigned int configured_ttl
);

#ifdef __cplusplus
}
#endif

#endif /* DISTRIBUTE_MESSAGES_H */

