
#include "with_payload_length.h"
#include <string.h>
#include "../../../lib/clone_str.h"
#include "../../../lib/emalloc.h"
#include "../session_storage.h"

/**
 * Precalculates the frame length to support output stream buffering.
 * The messages are in STOMP format, closed by an extra \0 byte, optionally wrapped in Websocket frame,
 * which can have a dynamic length header
 * 
 * TODO: optimization: the calculated length could come from outside
 * this way strlen does not have to be run again on the message content.
*/
message_with_frame_len * message_create_with_frame_len(int fd, char * str) {
    return message_create_with_frame_len_n(fd, str, strlen(str));
}

message_with_frame_len * message_create_with_frame_len_n(int fd, const char * bytes, size_t str_len) {
    message_with_frame_len * new_m = emalloc(sizeof (message_with_frame_len));

    size_t len = str_len;
    if(fd>0){ 
        len+=1; // stomp message ends with \0
        if(session_is_encoded(fd)) { // websocket frame header for a payload of len bytes
            if (len < 126) len += 2;
            else if (len < 65536) len += 4;
            else len += 10;
        } 
    }
    new_m->content = emalloc(str_len + 1); // zeroed: the STOMP NUL is in place
    if (str_len > 0) memcpy(new_m->content, bytes, str_len);
    new_m->content_len = str_len;
    new_m->fd = fd;
    new_m->frame_len = len;

    return new_m;
}

message_with_frame_len * message_create_raw(int fd, const char * bytes, size_t len, int close_after) {
    message_with_frame_len * new_m = emalloc(sizeof (message_with_frame_len));

    new_m->content = emalloc(len + 1);
    memcpy(new_m->content, bytes, len);
    new_m->content_len = len;
    new_m->fd = fd;
    new_m->frame_len = len;
    new_m->raw = 1;
    new_m->close_after = close_after;

    return new_m;
}

void message_destroy_with_frame_len(message_with_frame_len *m){
    free(m->content);
    free(m);
}

message_with_frame_len * message_poison_pill_with_frame_len() {
    return message_create_with_frame_len(-1, "KILL");
}