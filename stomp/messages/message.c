
#include <string.h>

#include "message.h"
#include "../../lib/emalloc.h"
#include "../../lib/clone_str.h"

char * ERROR_TEMPLATE = "ERROR\n"
        "%s%s%s" // receipt-id
        "%s%s%s" // version
        "content-type:text/plain\n"
        "content-length:%zu\n"
        "message:%s\n\n%s";

int len_of_int(unsigned int len) {
    if (len == 0) return 1;
    int digits = 0;
    for (int counter = len; counter > 0; counter = counter / 10) digits++;

    return digits;
}

/** ERROR frames always end the connection (STOMP 1.2): close_after is set. */
static message_with_frame_len * error_frame(int fd, const char *reason, const char *receipt_id,
        const char *versions) {
    const char *r1 = receipt_id ? "receipt-id:" : "", *r2 = receipt_id ? receipt_id : "", *r3 = receipt_id ? "\n" : "";
    const char *v1 = versions ? "version:" : "", *v2 = versions ? versions : "", *v3 = versions ? "\n" : "";
    size_t content_length = strlen(reason);
    int len = snprintf(NULL, 0, ERROR_TEMPLATE, r1, r2, r3, v1, v2, v3, content_length, reason, reason);

    char * frame = emalloc((size_t) len + 1);
    snprintf(frame, (size_t) len + 1, ERROR_TEMPLATE, r1, r2, r3, v1, v2, v3, content_length, reason, reason);

    message_with_frame_len * m = message_create_with_frame_len(fd, frame);
    m->close_after = 1;
    free(frame);

    return m;
}

message_with_frame_len * message_error(int fd, const char *reason, const char *receipt_id) {
    return error_frame(fd, reason, receipt_id, NULL);
}

message_with_frame_len * message_error_versions(int fd, const char *reason, const char *receipt_id,
        const char *supported_versions) {
    return error_frame(fd, reason, receipt_id, supported_versions);
}

char * CONNECTED_TEMPLATE = "CONNECTED\n"
        "version:%s\n"
        "server:kisp-stomp/0.0.1\n"
        "content-type:text/plain\n"
        "session:%d\n"
        "heart-beat:0,0\n\n"; // heart-beat not supported yet

message_with_frame_len * message_connected(int fd, int session_id, const char *version) {
    int len = snprintf(NULL, 0, CONNECTED_TEMPLATE, version, session_id);

    char * frame = emalloc((size_t) len + 1);
    snprintf(frame, (size_t) len + 1, CONNECTED_TEMPLATE, version, session_id);

    message_with_frame_len * m = message_create_with_frame_len(fd, frame);

    free(frame);
    return m;
}

char * DISCONNECT_TEMPLATE = "DISCONNECT\n";

message_with_frame_len * message_disconnect(int fd) {
    return message_create_with_frame_len(fd, DISCONNECT_TEMPLATE);
}

message_with_frame_len * message_close_request(int fd) {
    return message_create_raw(fd, "", 0, 1);
}

char * RECEIPT_TEMPLATE = "RECEIPT\n"
        "receipt-id:%s\n\n";

message_with_frame_len * message_receipt(int fd, char* receipt_id) {
    int len = (int) (strlen(RECEIPT_TEMPLATE) - 2);
    int digits = strlen(receipt_id);

    char * frame = emalloc(len + digits + 1);
    sprintf(frame, RECEIPT_TEMPLATE, receipt_id);

    message_with_frame_len * m = message_create_with_frame_len(fd, frame);
    free(frame);
    return m;
}

char * MESSAGE_TEMPLATE = "MESSAGE\n"
        "content-type:text/plain\n"
        "content-length:%d\n"
        "subscription:%d\n"
        "message-id:%d\n"
        "destination:%s\n\n%s";

char* MESSAGE_TEMPLATE_WITH_HEADERS = "MESSAGE\n"
        "%s\n\n%s";

message_with_frame_len * message_send_with_headers(int fd, associative_array * aa_headers,
        const char* body, size_t body_len) {
    char* headers = aa_create_str_representation(aa_headers->root);
    int head_len = snprintf(NULL, 0, MESSAGE_TEMPLATE_WITH_HEADERS, headers, "");

    char * frame = emalloc((size_t) head_len + body_len + 1);
    sprintf(frame, MESSAGE_TEMPLATE_WITH_HEADERS, headers, "");
    if (body_len > 0) memcpy(frame + head_len, body, body_len); // body may contain '\0'
    free(headers);
    message_with_frame_len * m = message_create_with_frame_len_n(fd, frame, (size_t) head_len + body_len);
    free(frame);
    return m;
}

char * MESSAGE_DIAGNOSTIC_TEMPLATE = "DIAG\n"
        "content-type:text/plain\n"
        "%s:%s\n\n";

message_with_frame_len * message_diagnostic(int fd, char * key, char * value) {
    int len = (int) (strlen(MESSAGE_DIAGNOSTIC_TEMPLATE) - 4); // without the two "%s"
    int total_len = len + strlen(key) + strlen(value);

    char * frame = emalloc(total_len + 1);
    sprintf(frame, MESSAGE_DIAGNOSTIC_TEMPLATE, key, value);

    message_with_frame_len *m = message_create_with_frame_len(fd, frame);
    free(frame);
    return m;
}
