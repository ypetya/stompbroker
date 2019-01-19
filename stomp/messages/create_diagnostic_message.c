
#include "create_diagnostic_message.h"
#include "../../lib/associative_array.h"

char msg_buffer[4000];

message_with_frame_len *create_diagnostic_message(message_with_timestamp *input,
                                                  parsed_message *pm,
                                                  ts_queue *q_in,
                                                  ts_queue *q_out) {
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