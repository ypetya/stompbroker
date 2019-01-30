
#include "with_payload_length.h"
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
    message_with_frame_len * new_m = emalloc(sizeof (message_with_frame_len));

    size_t len = strlen(str);
    if(fd>0){ 
        if(session_is_encoded(fd)) { // calculate websocket message frame length
            int header_len = 0;
            if (len < 126) header_len = 2;
            else if (len < 65536) header_len = 4;
            else header_len = 10;
            len+=header_len;
        } 
        len+=1; // stomp message ends with \0
    }
    new_m->content = clone_str_len(str, len);
    new_m->fd = fd;
    new_m->frame_len = len;

    return new_m;
}

void message_destroy_with_frame_len(message_with_frame_len *m){
    free(m->content);
    free(m);
}

message_with_frame_len * message_poison_pill_with_frame_len() {
    return message_create_with_frame_len(-1, "KILL");
}