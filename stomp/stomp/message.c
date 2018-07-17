
#include <string.h>

#include "message.h"
#include "../../lib/emalloc.h"
#include "../../lib/clone_str.h"

message * message_create(int fd, char * str, int len) {
    message * new_m = (message*) emalloc(sizeof (message));

    new_m->content = clone_str_len(str, len);
    new_m->fd = fd;

    return new_m;
}

void message_destroy(message * m) {
    free(m->content);
    free(m);
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

    return message_create(fd, frame, len + digits);
}

char * DISCONNECT_TEMPLATE = "DISCONNECT\n";

message * message_disconnect(int fd) {
    return message_create(fd, DISCONNECT_TEMPLATE, strlen(DISCONNECT_TEMPLATE));
}

message * message_message(int fd, int destination, int message_id, char * subscription) {

    return NULL;
}