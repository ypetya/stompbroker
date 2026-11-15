#include "distribute_messages.h"
#include "../data_wrappers/pub_sub.h"
#include "../../lib/queue.h"
#include "../../logger.h"
#include "../../parse_args.h"
#include "../../server/data/message/with_payload_length.h"
#include "create_diagnostic_message.h"
#include <time.h>

char * itoa(int num);

void add_next_message_id(associative_array * headers);

general_list * pick_next(general_list * subscriptions);

/* ======================================================================
 * MESSAGE frames
 * ====================================================================== */

/** Copies the SEND headers a MESSAGE carries on (not the sender's receipt request). */
static void copy_forwarded_headers(associative_array * target, aa_item * tree) {
    if (tree == NULL) return;
    if (strcmp(tree->key, "receipt") != 0) aa_put(target, tree->key, tree->value);
    copy_forwarded_headers(target, tree->left);
    copy_forwarded_headers(target, tree->right);
}

/** Headers shared by every MESSAGE made of pm; per subscription headers are added later. */
static associative_array * message_headers_of(parsed_message * pm) {
    associative_array * headers = aa_create();
    copy_forwarded_headers(headers, pm->headers->root);
    if (!aa_has(headers->root, "content-type")) aa_put(headers, "content-type", "text/plain");
    aa_put(headers, "content-length", itoa((int) pm->body_len));
    return headers;
}

static message_with_frame_len * message_for(associative_array * headers, parsed_message * pm,
        int session_id, char * subscription_id, unsigned int generation) {
    add_next_message_id(headers);
    aa_put(headers, "destination", pm->topic);
    aa_put(headers, "subscription", subscription_id);

    message_with_frame_len * o = message_send_with_headers(session_id, headers,
            pm->message_body, pm->body_len);
    o->generation = generation;
    return o;
}

/* ======================================================================
 * Stale queue: messages without subscribers, kept for TTL
 * ====================================================================== */

static size_t stale_bytes = 0;
size_t stale_queue_max_bytes = DEFAULT_STALE_QUEUE_BYTES;

size_t stale_queue_bytes(void) {
    return stale_bytes;
}

/** Memory a stored message is charged with. */
static size_t stale_size_of(parsed_message * pm) {
    return sizeof (parsed_message) + pm->body_len + strlen(pm->topic);
}

static int is_expired(parsed_message * pm, unsigned int ttl, long long now) {
    return pm->ts != 0 && ttl != 0 && now - pm->ts > (long long) ttl;
}

static void stale_free(parsed_message * pm) {
    stale_bytes -= stale_size_of(pm);
    free_parsed_message(pm);
}

/** Drops expired messages from the head (they are stored in receive order). */
static void stale_drop_expired(queue * stale_queue, unsigned int ttl, long long now) {
    while (stale_queue->first != NULL && is_expired(stale_queue->first->data, ttl, now))
        stale_free(dequeue(stale_queue));
}

/** @return 1 if pm is stored (the stale queue owns it now), 0 if it is dropped */
static int stale_store(queue * stale_queue, parsed_message * pm, unsigned max_count, unsigned int ttl) {
    stale_drop_expired(stale_queue, ttl, message_timestamp_now());
    size_t size = stale_size_of(pm);
    if ((unsigned) stale_queue->size >= max_count || stale_bytes + size > stale_queue_max_bytes) {
        warn("server: stale queue is full (%d messages, %zu bytes), dropping a message to %s\n",
                stale_queue->size, stale_bytes, pm->topic);
        return 0;
    }
    enqueue(stale_queue, pm);
    stale_bytes += size;
    return 1;
}

void stale_queue_clear(queue * stale_queue) {
    parsed_message * pm;
    while ((pm = dequeue(stale_queue)) != NULL) stale_free(pm);
}

/* ======================================================================
 * Distribution
 * ====================================================================== */

/**
 * Matches message destination topics against existing subrscriptions. If there 
 * is a match, message is put into the output_queue.
 * If there is no match and TTL > 0, message is put into stale_queue.
 * 
 * supporting load-balancing like message distributions:
 * There is a maintained counter which is increased by every message towards
 * any of the distributed topics. If the modulo of the index of a listeners 
 * matches this counter it will be chosen as the single consumer of the message
 * (sent to the matching distributed topic)
 * 
 * @return STOMP_MESSAGE_CONSUMED if pm can be freed, STOMP_MESSAGE_NOT_CONSUMED if the stale queue keeps it
 */
int distribute_messages(
        ts_queue* input_queue,
        unsigned max_stale_queue_size,
        queue * stale_queue,
        ts_queue* output_queue,
        message_with_timestamp *input,
        parsed_message * pm,
        unsigned int ttl) {
    // Drop a message that waited in the input queue for longer than TTL
    if (is_expired(pm, ttl, message_timestamp_now())) return STOMP_MESSAGE_CONSUMED;

    general_list * matching_clients = list_new();
    pubsub_find_matching(pm->topic, matching_clients);
    if (*pm->topic == '#' && matching_clients->size > 0)
        matching_clients = pick_next(matching_clients);

    int ret = STOMP_MESSAGE_CONSUMED;
    if (matching_clients->size > 0) {
        associative_array * headers = message_headers_of(pm);
        // Handle special topic, diagnostic messages
        if (strncmp(pm->topic, "DIAG", 4) == 0)
            create_diagnostic_headers(headers, pm->message_body,
                input_queue, output_queue, stale_queue);

        general_list * messages_out = list_new();
        for (general_list_item * c = matching_clients->first; c != NULL; c = c->next) {
            subscription * sub = c->data;
            list_add(messages_out, message_for(headers, pm, sub->session_id, sub->id, sub->generation));
        }
        ts_enqueue_multiple(output_queue, messages_out);

        aa_free(headers);
        list_clear(messages_out);
        free(messages_out);
    } else if (ttl > 0 && stale_store(stale_queue, pm, max_stale_queue_size, ttl)) {
        ret = STOMP_MESSAGE_NOT_CONSUMED;
    }

    list_clear(matching_clients);
    free(matching_clients);

    return ret;
}

/**
 * A new subscription takes the stored messages it matches (in order);
 * expired messages are dropped on the way.
 */
void distribute_messages_from_stale_q(
        char * topic, int client_session_id, char* subscription_id, unsigned int generation,
        queue * stale_queue,
        ts_queue* output_queue,
        unsigned int configured_ttl
        ) {
    general_list_item * parent = NULL;
    general_list_item * q_item = stale_queue->first;
    general_list * messages_out = list_new();
    long long now = message_timestamp_now();

    while (q_item) {
        parsed_message * pm = (parsed_message *) q_item->data;
        general_list_item * next = q_item->next;
        int expired = is_expired(pm, configured_ttl, now);

        if (expired || pubsub_topic_matches(topic, pm->topic)) {
            unchain_child(stale_queue, parent, q_item);
            free(q_item);
            if (!expired) {
                associative_array * headers = message_headers_of(pm);
                list_add(messages_out, message_for(headers, pm, client_session_id, subscription_id, generation));
                aa_free(headers);
            }
            stale_free(pm);
        } else {
            parent = q_item;
        }
        q_item = next;
    }

    if (messages_out->size > 0)
        ts_enqueue_multiple(output_queue, messages_out);

    list_clear(messages_out);
    free(messages_out);
}

char str_buf[20];

char * itoa(int num) {
    snprintf(str_buf, 20, "%d", num);
    return str_buf;
}

/**
 * message_id is incremented with every sent out message
 */
unsigned int message_id = 0;

void add_next_message_id(associative_array * headers) {
#ifdef DEBUG
    aa_put(headers, "message-id", itoa(message_id));
#else
    aa_put(headers, "message-id", itoa(message_id++));
#endif
}


unsigned int next_counter = 0;

general_list * pick_next(general_list * subscriptions) {
    general_list * messages_out = list_new();

    int next_index = next_counter++ % subscriptions->size;

    void * data = list_unchain_at(subscriptions, next_index);

    list_add(messages_out, data);

    list_clear(subscriptions);
    free(subscriptions);

    return messages_out;
}
