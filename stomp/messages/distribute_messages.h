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
#include "../data_wrappers/pub_sub.h"

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

/**
 * TTL == 0: drops the stored worker jobs ('#') that no subscription matches
 * any more (they waited for busy workers that are gone; nothing would expire
 * them). Call it after subscriptions are removed. TTL > 0: nothing to do.
 */
void stale_drop_orphaned_jobs(queue * stale_queue, unsigned int ttl);

/** Frees every stored message. */
void stale_queue_clear(queue * stale_queue);

/**
 * A new subscription (or one with freed credit) takes the stored messages it
 * matches, in order: all pub-sub messages, worker queue ('#') messages while it
 * has credit. Expired messages are dropped on the way.
 */
void distribute_messages_from_stale_q(subscription * sub, queue * stale_queue, ts_queue * output_queue,
    unsigned int configured_ttl);

/*
 * Worker queue acknowledgements (subscriptions with ack:client or
 * ack:client-individual). Settled messages free credit, so the owner gets
 * stored messages it matches.
 */

/** ACK (nack == 0) or NACK of the session's delivery ack_id; unknown ids are ignored. A NACKed message goes to another worker when one has credit. */
void worker_acknowledge(int session_id, unsigned long long ack_id, int nack,
    queue * stale_queue, ts_queue * output_queue, unsigned int ttl);

/** Redelivers the messages not acknowledged within ack_timeout micro-seconds (0: never). */
void worker_redeliver_timed_out(long long now, unsigned int ack_timeout,
    queue * stale_queue, ts_queue * output_queue, unsigned int ttl);

/** Redelivers the messages of pending entries whose subscriptions are gone (see pending_take_of); frees taken. */
void worker_redeliver_released(general_list * taken, queue * stale_queue, ts_queue * output_queue, unsigned int ttl);

#ifdef __cplusplus
}
#endif

#endif /* DISTRIBUTE_MESSAGES_H */

