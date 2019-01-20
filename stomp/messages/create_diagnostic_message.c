
#include "create_diagnostic_message.h"
#include "../../lib/associative_array.h"

char msg_buffer[4000];

/**
 * Getting diagnostic out from the broker via a special message frame
 * Where the frame command is DIAG
 * and the message's body is a simple string asking for statistics
 * 
 * This is a non standard STOMP message, so this interface can be used only
 * via non-stomp libraries.
*/
message_with_frame_len *create_diagnostic_message(message_with_timestamp *input,
                                                  parsed_message *pm,
                                                  ts_queue *q_in,
                                                  ts_queue *q_out)
{
    message_with_frame_len *resp = NULL;
    debug("Diagnostic query %s\n", pm->message_body);
    memset(msg_buffer, 0, 4000);
    if (strncmp(pm->message_body, "session-size", 12) == 0)
    {
        sprintf(msg_buffer, "%d", session_storage_size());
        resp = message_diagnostic(input->fd, pm->message_body, msg_buffer);
    }
    else if (strncmp(pm->message_body, "session-encoded-size", 71 - 51) == 0)
    {
        sprintf(msg_buffer, "%d", session_storage_encoded_size());
        resp = message_diagnostic(input->fd, pm->message_body, msg_buffer);
    }
    else if (strncmp(pm->message_body, "session-connected-size", 73 - 51) == 0)
    {
        sprintf(msg_buffer, "%llu", stomp_session_connected_size());
        debug("connected %llu\n", stomp_session_connected_size());
        resp = message_diagnostic(input->fd, pm->message_body, msg_buffer);
    }
    else if (strncmp(pm->message_body, "pubsub-size", 11) == 0)
    {
        sprintf(msg_buffer, "%d", pubsub_size());
        resp = message_diagnostic(input->fd, pm->message_body, msg_buffer);
    }
    else if (strncmp(pm->message_body, "session", 7) == 0)
    {
        sprintf(msg_buffer, "{\"clients\":%d,\"websockets\": %d,\"stomp_connected\":%d,\"subscriptions\":%d}",
                session_storage_size(),
                session_storage_encoded_size(),
                stomp_session_connected_size(),
                pubsub_size());
        resp = message_diagnostic(input->fd, pm->message_body, msg_buffer);
    }
    else if (strncmp(pm->message_body, "subs", 4) == 0)
    {
        pubsub_to_str(msg_buffer, 4000);
        resp = message_diagnostic(input->fd, pm->message_body, msg_buffer);
    }
    else if (strncmp(pm->message_body, "ws_buffer", 9) == 0)
    {
        struct ws_buffer_stat_t *stats = ws_buffer_get_stats();
        sprintf(msg_buffer, "{\"allocated\":%llu,\"hits\":%llu,\"misses\":%llu}",
                &stats->allocated_size, &stats->hit, &stats->miss);
        resp = message_diagnostic(input->fd, pm->message_body, msg_buffer);
    }
    else if (strncmp(pm->message_body, "stale", 5) == 0)
    {
        sprintf(msg_buffer, "{\"in\":%d,\"out\":%d}", q_in->q.size, q_out->q.size);
        resp = message_diagnostic(input->fd, pm->message_body, msg_buffer);
    }
    else
    {
        resp = message_error(input->fd, "Invalid message!");
    }

    return resp;
}

/**
 * Getting diagnostics via STOMP:
 * it is possible to subscribe to a special topic, "DIAG" where to 
 * every message sent can have a special message body to ask for a stat. 
 * 
 * The answers then will get appended to headers
*/
void create_diagnostic_headers(associative_array *headers, char *message_body,
                               ts_queue *q_in,
                               ts_queue *q_out)
{
    debug("Diagnostic query %s\n", message_body);
    memset(msg_buffer, 0, 4000);
    if (strncmp(message_body, "session-size", 12) == 0)
    {
        sprintf(msg_buffer, "%d", session_storage_size());
        aa_put(headers, "session-size", msg_buffer);
    }
    else if (strncmp(message_body, "session-encoded-size", 71 - 51) == 0)
    {
        sprintf(msg_buffer, "%d", session_storage_encoded_size());
        aa_put(headers, "session-encoded-size", msg_buffer);
    }
    else if (strncmp(message_body, "session-connected-size", 73 - 51) == 0)
    {
        sprintf(msg_buffer, "%llu", stomp_session_connected_size());
        aa_put(headers, "session-connected-size", msg_buffer);
    }
    else if (strncmp(message_body, "pubsub-size", 11) == 0)
    {
        sprintf(msg_buffer, "%d", pubsub_size());
        aa_put(headers, "pubsub-size", msg_buffer);
    }
    else if (strncmp(message_body, "session", 7) == 0)
    {
        sprintf(msg_buffer, "%d", session_storage_size());
        aa_put(headers, "clients", msg_buffer);
        sprintf(msg_buffer, "%d", session_storage_encoded_size());
        aa_put(headers, "websockets", msg_buffer);
        sprintf(msg_buffer, "%d", stomp_session_connected_size());
        aa_put(headers, "stomp_connected", msg_buffer);
        sprintf(msg_buffer, "%d", pubsub_size());
        aa_put(headers, "subscriptions", msg_buffer);
    }
    else if (strncmp(message_body, "ws_buffer", 9) == 0)
    {
        struct ws_buffer_stat_t *stats = ws_buffer_get_stats();
        sprintf(msg_buffer, "%llu", &stats->allocated_size);
        aa_put(headers, "allocated", msg_buffer);
        sprintf(msg_buffer, "%llu", &stats->hit);
        aa_put(headers, "hits", msg_buffer);
        sprintf(msg_buffer, "%llu", &stats->miss);
        aa_put(headers, "misses", msg_buffer);
    }
    else if (strncmp(message_body, "stale", 5) == 0)
    {
        sprintf(msg_buffer, "%d", q_in->q.size);
        aa_put(headers, "input-queue-size", msg_buffer);
        sprintf(msg_buffer, "%d", q_out->q.size);
        aa_put(headers, "output-queue-size", msg_buffer);
    }
}
