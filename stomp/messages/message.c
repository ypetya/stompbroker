
#include <string.h>

#include "message.h"
#include "../../lib/emalloc.h"
#include "../../lib/clone_str.h"

message_with_timestamp * message_poison_pill() {
    return message_create_with_timestamp(-1, "KILL", 0);
}

message_with_frame_len * message_wl_poison_pill() {
    return message_create_with_frame_len(-1, "KILL");
}

char * ERROR_TEMPLATE = "ERROR\n"
        "content-type:text/plain\n"
        "content-length:%d\n\n%s\n";

int len_of_int(unsigned int len) {
    if (len == 0) return 1;
    int digits = 0;
    for (int counter = len; counter > 0; counter = counter / 10) digits++;

    return digits;
}

message_with_frame_len * message_error(int fd, char *reason) {

    int content_length = strlen(reason);
    size_t len = strlen(reason) + strlen(ERROR_TEMPLATE) + 10;
    char * frame = emalloc(len);
    sprintf(frame, ERROR_TEMPLATE, content_length, reason);

    message_with_frame_len * m = message_create_with_frame_len(fd, frame);
    free(frame);

    return m;
}

char * CONNECTED_TEMPLATE = "CONNECTED\n"
        "version:1.2\n"
        "server:kisp-stomp/0.0.1\n"
        "content-type:text/plain\n"
        "session:%d\n"
        "heart-beat:0,0\n\n"; // heart-beat not supported yet

message_with_frame_len * message_connected(int fd, int session_id) {
    int len = (int) (strlen(CONNECTED_TEMPLATE) - 2);
    int digits = len_of_int(session_id);

    char * frame = emalloc(len + digits + 1);
    sprintf(frame, CONNECTED_TEMPLATE, session_id);

    message_with_frame_len * m = message_create_with_frame_len(fd, frame);

    free(frame);
    return m;
}

char * DISCONNECT_TEMPLATE = "DISCONNECT\n";

message_with_frame_len * message_disconnect(int fd) {
    return message_create_with_frame_len(fd, DISCONNECT_TEMPLATE);
}

char * RECEIPT_TEMPLATE = "RECEIPT\n"
        "content-type:text/plain\n"
        "receipt-id:%s\n";

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
        char* body) {
    char* headers = aa_create_str_representation(aa_headers->root);
    int template_len = (int) (strlen(MESSAGE_TEMPLATE_WITH_HEADERS) - 4);
    int total_len = template_len + strlen(headers) + strlen(body);

    char * frame = emalloc(total_len + 1);
    sprintf(frame, MESSAGE_TEMPLATE_WITH_HEADERS, headers, body);
    free(headers);
    message_with_frame_len * m = message_create_with_frame_len(fd, frame);
    free(frame);
    return m;
}

char * MESSAGE_DIAGNOSTIC_TEMPLATE = "DIAG\n"
        "content-type:text/plain\n"
        "%s:%s\n";

message_with_frame_len * message_diagnostic(int fd, char * key, char * value) {
    int len = (int) (strlen(MESSAGE_DIAGNOSTIC_TEMPLATE) - 4);
    int total_len = len + strlen(key) + strlen(value);

    char * frame = emalloc(total_len + 1);
    sprintf(frame, MESSAGE_DIAGNOSTIC_TEMPLATE, key, value);

    message_with_frame_len *m = message_create_with_frame_len(fd, frame);
    free(frame);
    return m;
}
