
#include <string.h>

#include "message.h"
#include "../../lib/emalloc.h"
#include "../../lib/clone_str.h"

message * message_create(int fd, char * str, int len) {
    message * new_m = emalloc(sizeof (message));

    new_m->content = clone_str_len(str, len);
    new_m->fd = fd;

    return new_m;
}

void message_destroy(message * m) {
    free(m->content);
    free(m);
}

message * message_poison_pill() {
    return message_create(-1, "KILL", 4);
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

message * message_error(int fd, char *reason) {

    int len = (int) (strlen(reason) + strlen(ERROR_TEMPLATE) - 4);
    int digits = len_of_int(len);
    char * frame = emalloc(len + digits + 1);
    sprintf(frame, ERROR_TEMPLATE, len + digits, reason);
    debug("Constucting ERROR frame:\n%s", frame); 
    return message_create(fd, frame, len + digits);
}

char * CONNECTED_TEMPLATE = "CONNECTED\n"
        "content-type:text/plain\n"
        "content-length:%d\n"
        "session:%d\n"
        "server:kisp-stomp\n"
        "heart-beat:0,0\n"; // heart-beat not supported yet

message * message_connected(int fd, int session_id) {
    int len = (int) (strlen(CONNECTED_TEMPLATE) - 4);
    int digits = len_of_int(len) + len_of_int(session_id);

    char * frame = emalloc(len + digits + 1);
    sprintf(frame, CONNECTED_TEMPLATE, len + digits, session_id);
    debug("---\n%s", frame); 
    return message_create(fd, frame, len + digits);
}

char * DISCONNECT_TEMPLATE = "DISCONNECT\n";

message * message_disconnect(int fd) {
    debug("---\nDISCONNECT"); 
    return message_create(fd, DISCONNECT_TEMPLATE, strlen(DISCONNECT_TEMPLATE));
}

char * RECEIPT_TEMPLATE = "RECEIPT\n"
        "content-type:text/plain\n"
        "content-length:%d\n"
        "receipt-id:%s\n";

message * message_receipt(int fd, char* receipt_id) {
    int len = (int) (strlen(RECEIPT_TEMPLATE) - 4);
    int digits = len_of_int(len) + strlen(receipt_id);

    char * frame = emalloc(len + digits + 1);
    sprintf(frame, RECEIPT_TEMPLATE, len + digits, receipt_id);
    debug("---\n%s", frame); 
    return message_create(fd, frame, len + digits);
}

char * MESSAGE_TEMPLATE = "MESSAGE\n"
        "content-type:text/plain\n"
        "content-length:%d\n"
        "subscription:%d\n"
        "message-id:%d\n"
        "destination:%s\n\n%s";

message * message_send(int fd, int subscription_id, int message_id, char* dest,
        char* body) {
    int len = (int) (strlen(MESSAGE_TEMPLATE) - 10);
    int digits = len_of_int(len) + len_of_int(subscription_id)
            + len_of_int(message_id) + strlen(dest) + strlen(body);

    char * frame = emalloc(len + digits + 1);
    sprintf(frame, MESSAGE_TEMPLATE, len + digits, subscription_id, message_id, dest, body);
    debug("---\n%s", frame);
    return message_create(fd, frame, len + digits);
}

char * MESSAGE_DIAGNOSTIC_TEMPLATE="DIAG\n"
    "content-type:text/plain\n"
    "content-length:%d\n"
    "%s:%s\n";

message * message_diagnostic(int fd, char * key, char * value) {
    int len = (int) (strlen(MESSAGE_DIAGNOSTIC_TEMPLATE) - 4);
    int digits = len_of_int(len) + strlen(key) + strlen(value);

    char * frame = emalloc(len + digits + 1);
    sprintf(frame, MESSAGE_DIAGNOSTIC_TEMPLATE, len + digits, key, value);
    debug("---\n%s", frame);
    return message_create(fd, frame, len + digits);
}
    