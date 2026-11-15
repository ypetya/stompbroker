
#include "stomp.h"
#include "../logger.h"

#include "../parse_args.h"

#include "../lib/thread_safe_queue.h"
#include "../server/data/session_storage.h"
#include "data_wrappers/session.h"
#include "parser.h"

#include "messages/message.h"
#include "messages/create_diagnostic_message.h"
#include "messages/distribute_messages.h"

/**
 * Time to live, from config at startup
 * */
unsigned int ttl;
unsigned int stale_queue_max_size;

#define SUPPORTED_VERSIONS "1.0,1.1,1.2"

/** What a frame handler answers. */
typedef struct stomp_reply_st {
    message_with_frame_len * frame; // reply frame, NULL: none (RECEIPT is added when requested)
    int close; // close the connection after the reply (DISCONNECT)
    int consumed; // the parsed frame can be freed (not kept in the stale queue)
} stomp_reply;

typedef struct stomp_request_st {
    message_with_timestamp * input;
    parsed_message * pm;
    int client_id; // fd with session flags
    int connected;
    ts_queue * input_queue;
    ts_queue * output_queue;
    queue * stale_queue;
} stomp_request;

static stomp_reply answer(message_with_frame_len * frame) {
    return (stomp_reply) {frame, 0, STOMP_MESSAGE_CONSUMED};
}

static stomp_reply fail(stomp_request * r, const char * reason) {
    return answer(message_error(r->input->fd, reason, r->pm->receipt_id));
}

static stomp_reply nothing(void) {
    return answer(NULL);
}

/** Queues a reply to input, for the session that sent it. */
static void reply_to(ts_queue * output_queue, message_with_timestamp * input, message_with_frame_len * m) {
    m->generation = input->generation;
    ts_enqueue(output_queue, m);
}

/** Is version one of the comma separated tokens of list? */
static int list_has_version(const char * list, const char * version) {
    size_t len = strlen(version);
    for (const char * p = list; *p != '\0';) {
        while (*p == ' ' || *p == ',') p++;
        const char * end = p;
        while (*end != '\0' && *end != ',') end++;
        const char * last = end;
        while (last > p && last[-1] == ' ') last--;
        if ((size_t) (last - p) == len && strncmp(p, version, len) == 0) return 1;
        p = end;
    }
    return 0;
}

/**
 * STOMP 1.2 version negotiation: the highest version both sides support.
 * A CONNECT without accept-version comes from a STOMP 1.0 client.
 * @return the version, NULL if there is none in common
 */
static const char * negotiate_version(parsed_message * pm) {
    aa_item * header = aa_get(pm->headers->root, "accept-version");
    if (header == NULL) return "1.0";
    if (list_has_version(header->value, "1.2")) return "1.2";
    if (list_has_version(header->value, "1.1")) return "1.1";
    if (list_has_version(header->value, "1.0")) return "1.0";
    return NULL;
}

static stomp_reply on_connect(stomp_request * r) {
    if (r->connected) return fail(r, "Can not connect, client is already connected!");

    const char * version = negotiate_version(r->pm);
    if (version == NULL)
        return answer(message_error_versions(r->input->fd, "Supported protocol versions are " SUPPORTED_VERSIONS,
            r->pm->receipt_id, SUPPORTED_VERSIONS));

    int session = session_without_flags(r->client_id);
    stomp_session_set_connected(session, 1);
    return answer(message_connected(r->input->fd, session, version));
}

static stomp_reply on_disconnect(stomp_request * r) {
    if (!r->connected) return fail(r, "Can not disconnect, client is not connected yet!");
    pubsub_remove_client(r->client_id);
    stomp_session_set_connected(session_without_flags(r->client_id), 0);
    return (stomp_reply) {NULL, 1, STOMP_MESSAGE_CONSUMED};
}

static stomp_reply on_subscribe(stomp_request * r) {
    parsed_message * pm = r->pm;
    if (!r->connected) return fail(r, "Not connected!");
    if (pm->id == NULL) return fail(r, "No id defined!");
    if (pm->topic == NULL) return fail(r, "No destination defined!");
    if (pubsub_has_subscription(r->client_id, pm->id)) return fail(r, "Subscription id is already in use!");

    pubsub_subscribe(pm->topic, r->client_id, pm->id, r->input->generation);
    distribute_messages_from_stale_q(pm->topic, r->client_id, pm->id, r->input->generation,
            r->stale_queue, r->output_queue, ttl);
    return nothing();
}

static stomp_reply on_unsubscribe(stomp_request * r) {
    if (!r->connected) return fail(r, "Not connected!");
    if (r->pm->id == NULL) return fail(r, "No id defined!");
    pubsub_unsubscribe(r->client_id, r->pm->id);
    return nothing();
}

static stomp_reply on_send(stomp_request * r) {
    parsed_message * pm = r->pm;
    if (!r->connected) return fail(r, "Not connected!");
    if (pm->topic == NULL) return fail(r, "No topic defined!");
    if (strchr(pm->topic, '*') != NULL) return fail(r, "Can not have wildcard in message destination!");

    stomp_reply reply = nothing();
    reply.consumed = distribute_messages(r->input_queue, stale_queue_max_size, r->stale_queue,
            r->output_queue, r->input, pm, ttl);
    return reply;
}

/** This DIAGNOSTIC frame returns internal info (non standard, no connection needed). */
static stomp_reply on_diagnostic(stomp_request * r) {
    if (r->pm->body_len == 0) return fail(r, "Empty message_body!");
    return answer(create_diagnostic_message(r->input, r->pm, r->input_queue, r->output_queue));
}

static stomp_reply handle(stomp_request * r) {
    switch (r->pm->command) {
        case FRM_CONNECT_ID: return on_connect(r);
        case FRM_DISCONNECT_ID: return on_disconnect(r);
        case FRM_SUBSCRIBE_ID: return on_subscribe(r);
        case FRM_UNSUBSCRIBE_ID: return on_unsubscribe(r);
        case FRM_SEND_ID: return on_send(r);
        case FRM_DIAGNOSTIC_ID: return on_diagnostic(r);
        default: return fail(r, r->pm->error ? r->pm->error : "Invalid message!");
    }
}

/** The session ends: no more deliveries to it, its connection gets closed. */
static void end_session(int client_id) {
    pubsub_remove_client(client_id);
    stomp_session_set_connected(session_without_flags(client_id), 0);
}

/**
 * Processing incoming STOMP message
 *
 * @return fd: to do cleanup for. 0: message processed
*/
int stomp_process(ts_queue* input_queue,
    queue* stale_queue,
    ts_queue* output_queue,
    message_with_timestamp *input) {

    int client_id = input->fd;
    int client_id_wo_flags = session_without_flags(input->fd);
    if (client_id != client_id_wo_flags && session_is_cmd_purge(client_id) > 0) {
        int session_with_other_flags = session_wo_cmd_purge(client_id);
        debug("Purge STOMP session fd: %d %s\n",client_id_wo_flags,session_is_encoded(client_id) ? "Websocket": "");
        end_session(session_with_other_flags);
        return client_id_wo_flags;
    }

    if (input->error != NULL) { // the connection's byte stream could not be cut into frames
        end_session(client_id);
        reply_to(output_queue, input, message_error(input->fd, input->error, NULL));
        return 0;
    }

    parsed_message * pm = parse_message(input);
    stomp_request request = {
        input, pm, client_id, stomp_session_is_connected(client_id_wo_flags),
        input_queue, output_queue, stale_queue
    };
    stomp_reply reply = handle(&request);

    message_with_frame_len * frame = reply.frame;
    if (frame == NULL && pm->receipt_id != NULL) frame = message_receipt(input->fd, pm->receipt_id);
    if (frame == NULL && reply.close) frame = message_close_request(input->fd);
    if (frame != NULL) {
        if (frame->close_after) end_session(client_id); // ERROR
        frame->close_after |= reply.close;
        reply_to(output_queue, input, frame);
    }

    if (reply.consumed) free_parsed_message(pm);

    return 0;
}

void stomp_start() {
    stomp_app_config* config = config_get_config();
    ttl = config->ttl;
    stale_queue_max_size = config->max_stale_queue_size;
    stale_queue_max_bytes = config->max_stale_queue_bytes;

    stomp_session_init();
    pubsub_init();
}

void stomp_stop(queue * stale_queue) {
    pubsub_dispose();
    stale_queue_clear(stale_queue);
}
