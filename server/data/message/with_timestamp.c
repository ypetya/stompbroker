#include "with_timestamp.h"
#include "../../../lib/clone_str.h"
#include "../../../lib/emalloc.h"
#include <time.h>
#include <string.h>

long long message_timestamp_now(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long long) now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

message_with_timestamp * message_create_with_timestamp(int fd, char * str, long long ts) {
    return message_create_with_timestamp_len(fd, str, strlen(str), ts);
}

message_with_timestamp * message_create_with_timestamp_len(int fd, const char * bytes, size_t len, long long ts) {
    message_with_timestamp * new_m = emalloc(sizeof (message_with_timestamp));

    new_m->content = emalloc(len + 1); // zeroed: content[len] == '\0'
    if (len > 0) memcpy(new_m->content, bytes, len);
    new_m->content_len = len;
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
