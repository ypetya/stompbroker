
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

char * ERROR_TEMPLATE = "ERROR\ncontent-type:text/plain\ncontent-length:%d\n\n%s";

message * message_error(int fd, char *reason) {

    int len = (int) (strlen(reason) + strlen(ERROR_TEMPLATE) - 4);
    int digits = 0;
    for (int counter = len; counter > 0; counter = counter / 10) digits++;
    char * frame = emalloc(len + digits + 1);
    sprintf(frame, ERROR_TEMPLATE, len + digits, reason);
    message * error = message_create(fd, frame, len + digits);

    printf(frame);

    return error;
}