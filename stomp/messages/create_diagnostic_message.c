
#include "create_diagnostic_message.h"
#include "../../lib/associative_array.h"

message_with_frame_len * create_diagnostic_message(message_with_timestamp *input, parsed_message * pm) {
    message_with_frame_len * resp = NULL;
    debug("Diagnostic query %s\n", pm->message_body);
    char buf[10];
    if (strncmp(pm->message_body, "session-size", 12) == 0) {
        sprintf(buf, "%d", session_storage_size());
        resp = message_diagnostic(input->fd, pm->message_body, buf);
    } else if (strncmp(pm->message_body, "session-encoded-size", 71 - 51) == 0) {
        sprintf(buf, "%d", session_storage_encoded_size());
        resp = message_diagnostic(input->fd, pm->message_body, buf);
    } else if (strncmp(pm->message_body, "session-connected-size", 73 - 51) == 0) {
        sprintf(buf, "%llu", stomp_session_connected_size());
        debug("connected %llu\n", stomp_session_connected_size());
        resp = message_diagnostic(input->fd, pm->message_body, buf);
    } else if (strncmp(pm->message_body, "pubsub-size", 11) == 0) {
        sprintf(buf, "%d", pubsub_size());
        resp = message_diagnostic(input->fd, pm->message_body, buf);
    } else if (strncmp(pm->message_body, "subs", 4) == 0) {
        char * large_buffer = emalloc(3500);
        pubsub_to_str(large_buffer, 3500);
        resp = message_diagnostic(input->fd, pm->message_body, large_buffer);
        free(large_buffer);
    } else if (strncmp(pm->message_body, "ws_buffer", 9)==0) {
        char * medium_buffer = emalloc(1000);
        struct ws_buffer_stat_t * stats = ws_buffer_get_stats();
        sprintf(medium_buffer, "Allocated:%llu\nHits:%llu\nMiss:%llu\n",
            &stats->allocated_size, &stats->hit, &stats->miss);
        resp = message_diagnostic(input->fd, pm->message_body, medium_buffer);
        free(medium_buffer);
    } else {
        resp = message_error(input->fd, "Invalid message!");
    }

    return resp;
}