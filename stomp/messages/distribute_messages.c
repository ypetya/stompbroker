#include "distribute_messages.h"
#include "../data_wrappers/pub_sub.h"
#include "../data_wrappers/pending_acks.h"
#include "../../lib/queue.h"
#include "../../logger.h"
#include "../../parse_args.h"
#include "../../server/data/message/with_payload_length.h"
#include "create_diagnostic_message.h"
#include <time.h>

char * itoa(int num);

void add_next_message_id(associative_array * headers);

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

static size_t str_size(const char * s) {
    return s != NULL ? strlen(s) + 1 : 0;
}

static size_t headers_size(const aa_item * node) {
    if (node == NULL) return 0;
    return sizeof (aa_item) + str_size(node->key) + str_size(node->value)
            + headers_size(node->left) + headers_size(node->right);
}

/**
 * Memory a stored message is charged with: everything it owns. Stored
 * messages are never changed, so storing and freeing charge the same size.
 */
static size_t stale_size_of(parsed_message * pm) {
    return sizeof (parsed_message) + sizeof (associative_array) + headers_size(pm->headers->root)
            + pm->body_len + 1 + str_size(pm->topic) + str_size(pm->receipt_id) + str_size(pm->id);
}

static int is_expired(parsed_message * pm, unsigned int ttl, long long now) {
    return pm->ts != 0 && ttl != 0 && now - pm->ts > (long long) ttl;
}

static void stale_free(parsed_message * pm) {
    stale_bytes -= stale_size_of(pm);
    free_parsed_message(pm);
}

/**
 * Drops expired messages from the head. Mostly they are stored in receive
 * order, but redelivered worker jobs go to the front (stale_put_front), so a
 * live job at the head may hide expired messages behind it: see
 * stale_drop_where.
 */
static void stale_drop_expired(queue * stale_queue, unsigned int ttl, long long now) {
    while (stale_queue->first != NULL && is_expired(stale_queue->first->data, ttl, now))
        stale_free(dequeue(stale_queue));
}

typedef int (*stale_filter)(parsed_message * pm, const void * ctx);

/** Drops every stored message matching the filter, wherever it is in the queue. */
static void stale_drop_where(queue * stale_queue, stale_filter matches, const void * ctx) {
    general_list_item * parent = NULL;
    general_list_item * item = stale_queue->first;
    while (item != NULL) {
        general_list_item * next = item->next;
        if (matches(item->data, ctx)) {
            unchain_child(stale_queue, parent, item);
            stale_free(item->data);
            free(item);
        } else {
            parent = item;
        }
        item = next;
    }
}

typedef struct expiry_st {
    unsigned int ttl;
    long long now;
} expiry;

static int expired_at(parsed_message * pm, const void * ctx) {
    const expiry * e = ctx;
    return is_expired(pm, e->ttl, e->now);
}

static int stale_is_full(queue * stale_queue, size_t size, unsigned max_count) {
    return (unsigned) stale_queue->size >= max_count || stale_bytes + size > stale_queue_max_bytes;
}

/** @return 1 if pm is stored (the stale queue owns it now), 0 if it is dropped */
static int stale_store(queue * stale_queue, parsed_message * pm, unsigned max_count, unsigned int ttl) {
    expiry now = {ttl, message_timestamp_now()};
    stale_drop_expired(stale_queue, ttl, now.now);
    size_t size = stale_size_of(pm);
    // full: expired messages behind a redelivered (younger) head have to go too
    if (stale_is_full(stale_queue, size, max_count) && ttl != 0)
        stale_drop_where(stale_queue, expired_at, &now);
    if (stale_is_full(stale_queue, size, max_count)) {
        warn("server: stale queue is full (%d messages, %zu bytes), dropping a message to %s\n",
                stale_queue->size, stale_bytes, pm->topic);
        return 0;
    }
    enqueue(stale_queue, pm);
    stale_bytes += size;
    return 1;
}

/** Unchains a stored message: the caller owns it now. */
static parsed_message * stale_take(queue * stale_queue, general_list_item * parent, general_list_item * item) {
    parsed_message * pm = item->data;
    unchain_child(stale_queue, parent, item);
    free(item);
    stale_bytes -= stale_size_of(pm);
    return pm;
}

/**
 * Redelivered messages go before the others, in their order and beyond the
 * limits: they were accepted before them.
 */
static void stale_put_front(queue * stale_queue, general_list * pms) {
    if (pms->size == 0) return;
    parsed_message ** in_order = emalloc(sizeof (parsed_message *) * (size_t) pms->size);
    int count = 0;
    for (general_list_item * c = pms->first; c != NULL; c = c->next) in_order[count++] = c->data;
    while (count > 0) {
        parsed_message * pm = in_order[--count];
        put_head(stale_queue, pm);
        stale_bytes += stale_size_of(pm);
    }
    free(in_order);
}

/** Worker queue destinations start with '#'. */
static int is_worker_queue(const char * destination) {
    return *destination == '#';
}

/** A stored worker job that no subscription matches any more. */
static int orphaned_job(parsed_message * pm, const void * ctx) {
    return is_worker_queue(pm->topic) && !pubsub_has_matching(pm->topic);
}

void stale_drop_orphaned_jobs(queue * stale_queue, unsigned int ttl) {
    if (ttl == 0) stale_drop_where(stale_queue, orphaned_job, NULL);
}

void stale_queue_clear(queue * stale_queue) {
    parsed_message * pm;
    while ((pm = dequeue(stale_queue)) != NULL) stale_free(pm);
}

/* ======================================================================
 * Worker queues: destinations starting with '#'
 * ====================================================================== */

/** Can the subscription take one more worker queue message? */
static int has_credit(const subscription * sub) {
    return sub->ack == ACK_AUTO || sub->in_flight < sub->prefetch;
}

static unsigned long long pick_sequence = 0;

/**
 * Round-robin: of the matching subscriptions that have credit (except
 * exclude), the one chosen least recently. Every subscription keeps one last
 * pick for all destinations it matches. Subscriptions to a single destination
 * rotate per destination; a wildcard subscription (e.g. '#*') is also picked
 * for other destinations, which moves its last pick and skews the rotation
 * of every destination it shares (known limitation, see pubsub.md).
 * @return NULL if none can take the message
 */
static subscription * pick_worker(general_list * matching, const subscription * exclude) {
    subscription * chosen = NULL;
    for (general_list_item * c = matching->first; c != NULL; c = c->next) {
        subscription * sub = c->data;
        if (sub != exclude && has_credit(sub) && (chosen == NULL || sub->last_pick < chosen->last_pick))
            chosen = sub;
    }
    return chosen;
}

/**
 * Adds the MESSAGE of pm for sub to out. A worker queue message to a
 * subscription with ack != auto waits in the pending store for its ACK.
 * @return STOMP_MESSAGE_CONSUMED if the caller still owns pm, STOMP_MESSAGE_NOT_CONSUMED if the pending store took it
 */
static int deliver_to(subscription * sub, parsed_message * pm, general_list * out) {
    associative_array * headers = message_headers_of(pm);
    int ret = STOMP_MESSAGE_CONSUMED;
    if (is_worker_queue(pm->topic)) {
        sub->last_pick = ++pick_sequence;
        if (sub->ack != ACK_AUTO) {
            char ack_id[24];
            snprintf(ack_id, sizeof (ack_id), "%llu", pending_add(sub, pm, message_timestamp_now()));
            aa_put(headers, "ack", ack_id);
            ret = STOMP_MESSAGE_NOT_CONSUMED;
        }
    }
    list_add(out, message_for(headers, pm, sub->session_id, sub->id, sub->generation));
    aa_free(headers);
    return ret;
}

static void flush(ts_queue * output_queue, general_list * out) {
    if (out->size > 0) ts_enqueue_multiple(output_queue, out);
    list_clear(out);
    free(out);
}

/**
 * Gives sub the stored messages it matches, in order: every pub-sub message,
 * and worker queue messages while sub has credit. Expired messages are dropped
 * on the way.
 */
static void take_stored_messages(subscription * sub, queue * stale_queue, unsigned int ttl, general_list * out) {
    general_list_item * parent = NULL;
    general_list_item * item = stale_queue->first;
    long long now = message_timestamp_now();

    while (item != NULL) {
        parsed_message * pm = item->data;
        general_list_item * next = item->next;
        int expired = is_expired(pm, ttl, now);
        int wanted = !expired && pubsub_topic_matches(sub->topic_pattern, pm->topic)
                && (!is_worker_queue(pm->topic) || has_credit(sub));

        if (expired || wanted) {
            stale_take(stale_queue, parent, item);
            if (!wanted || deliver_to(sub, pm, out) == STOMP_MESSAGE_CONSUMED) free_parsed_message(pm);
        } else {
            parent = item;
        }
        item = next;
    }
}

/**
 * One worker gets the message. If every matching worker is busy (no credit)
 * the message waits in the stale queue, even with TTL == 0; if there is no
 * matching subscription it is stored only when TTL > 0.
 */
static int distribute_to_worker(parsed_message * pm, general_list * matching, queue * stale_queue,
        unsigned max_stale_queue_size, unsigned int ttl, general_list * out) {
    subscription * worker = pick_worker(matching, NULL);
    if (worker != NULL) return deliver_to(worker, pm, out);
    int keep = matching->size > 0 || ttl > 0;
    return keep && stale_store(stale_queue, pm, max_stale_queue_size, ttl)
            ? STOMP_MESSAGE_NOT_CONSUMED : STOMP_MESSAGE_CONSUMED;
}

/**
 * A message that was not acknowledged goes to another worker if one has
 * credit; else it is added to waiting, to return to the stale queue.
 */
static void redeliver(parsed_message * pm, const subscription * exclude, unsigned int ttl,
        general_list * waiting, general_list * out) {
    if (is_expired(pm, ttl, message_timestamp_now())) {
        free_parsed_message(pm);
        return;
    }
    aa_put(pm->headers, "redelivered", "true");

    general_list * matching = list_new();
    pubsub_find_matching(pm->topic, matching);
    subscription * worker = pick_worker(matching, exclude);
    if (worker == NULL) list_add(waiting, pm);
    else if (deliver_to(worker, pm, out) == STOMP_MESSAGE_CONSUMED) free_parsed_message(pm);
    list_clear(matching);
    free(matching);
}

/**
 * Settles taken pending entries: an ACK drops the messages, a NACK, a time-out
 * or a removed subscription redelivers them (if owners_remain, to another
 * worker when possible). Owners that remain get stored messages for the
 * credit freed.
 */
static void settle(general_list * taken, int redeliver_messages, int owners_remain,
        queue * stale_queue, ts_queue * output_queue, unsigned int ttl) {
    general_list * out = list_new();
    general_list * waiting = list_new();
    for (general_list_item * c = taken->first; c != NULL; c = c->next) {
        pending_ack * entry = c->data;
        if (redeliver_messages) redeliver(entry->pm, owners_remain ? entry->sub : NULL, ttl, waiting, out);
        else free_parsed_message(entry->pm);
    }
    stale_put_front(stale_queue, waiting);
    list_clear(waiting);
    free(waiting);
    for (general_list_item * c = taken->first; c != NULL; c = c->next) {
        pending_ack * entry = c->data;
        if (owners_remain) take_stored_messages(entry->sub, stale_queue, ttl, out);
        free(entry);
    }
    list_clear(taken);
    free(taken);
    flush(output_queue, out);
}

void worker_acknowledge(int session_id, unsigned long long ack_id, int nack,
        queue * stale_queue, ts_queue * output_queue, unsigned int ttl) {
    settle(pending_take_acked(session_id, ack_id), nack, 1, stale_queue, output_queue, ttl);
}

void worker_redeliver_timed_out(long long now, unsigned int ack_timeout,
        queue * stale_queue, ts_queue * output_queue, unsigned int ttl) {
    if (ack_timeout == 0 || pending_size() == 0) return;
    settle(pending_take_sent_before(now - (long long) ack_timeout), 1, 1, stale_queue, output_queue, ttl);
}

void worker_redeliver_released(general_list * taken, queue * stale_queue, ts_queue * output_queue, unsigned int ttl) {
    settle(taken, 1, 0, stale_queue, output_queue, ttl);
}

/* ======================================================================
 * Distribution
 * ====================================================================== */

/** Every matching subscription gets the message; none: it is stored when TTL > 0. */
static int distribute_to_all(parsed_message * pm, general_list * matching,
        ts_queue * input_queue, queue * stale_queue, ts_queue * output_queue,
        unsigned max_stale_queue_size, unsigned int ttl, general_list * out) {
    if (matching->size == 0)
        return ttl > 0 && stale_store(stale_queue, pm, max_stale_queue_size, ttl)
                ? STOMP_MESSAGE_NOT_CONSUMED : STOMP_MESSAGE_CONSUMED;

    associative_array * headers = message_headers_of(pm);
    // Handle special topic, diagnostic messages
    if (strncmp(pm->topic, "DIAG", 4) == 0)
        create_diagnostic_headers(headers, pm->message_body, input_queue, output_queue, stale_queue);
    for (general_list_item * c = matching->first; c != NULL; c = c->next) {
        subscription * sub = c->data;
        list_add(out, message_for(headers, pm, sub->session_id, sub->id, sub->generation));
    }
    aa_free(headers);
    return STOMP_MESSAGE_CONSUMED;
}

/**
 * Matches the message destination against the subscriptions. Pub-sub
 * destinations go to every match; worker queue destinations ('#') to one
 * (see distribute_to_worker). Without a match and with TTL > 0 the message is
 * stored in the stale queue.
 *
 * @return STOMP_MESSAGE_CONSUMED if pm can be freed, STOMP_MESSAGE_NOT_CONSUMED if the stale queue or the pending store keeps it
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

    general_list * matching = list_new();
    pubsub_find_matching(pm->topic, matching);
    general_list * out = list_new();

    int ret = is_worker_queue(pm->topic)
            ? distribute_to_worker(pm, matching, stale_queue, max_stale_queue_size, ttl, out)
            : distribute_to_all(pm, matching, input_queue, stale_queue, output_queue, max_stale_queue_size, ttl, out);

    flush(output_queue, out);
    list_clear(matching);
    free(matching);
    return ret;
}

void distribute_messages_from_stale_q(subscription * sub, queue * stale_queue, ts_queue * output_queue,
        unsigned int configured_ttl) {
    general_list * out = list_new();
    take_stored_messages(sub, stale_queue, configured_ttl, out);
    flush(output_queue, out);
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
