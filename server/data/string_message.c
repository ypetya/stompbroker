
#include "string_message.h"
#include "../../lib/emalloc.h"

message * message_create(int fd, char * str) {
    message * new_m = emalloc(sizeof (message));

    new_m->content = clone_str(str);
    new_m->fd = fd;

    return new_m;
}

void message_destroy(message * m) {
    free(m->content);
    free(m);
}