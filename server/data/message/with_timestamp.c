#include "with_timestamp.h"
#include "../../../lib/clone_str.h"
#include "../../../lib/emalloc.h"
#include <time.h>

message_with_timestamp * message_create_with_timestamp(int fd, char * str, clock_t ts) {
    message_with_timestamp * new_m = emalloc(sizeof (message_with_timestamp));

    new_m->content = clone_str(str);
    new_m->fd = fd;
    new_m->ts = ts;

    return new_m;
}

void message_destroy_with_timestamp(message_with_timestamp * m) {
    free(m->content);
    free(m);
}

message_with_timestamp * message_poison_pill_with_timestamp() {
    return message_create_with_timestamp(-1, "KILL", 0);
}
