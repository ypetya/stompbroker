
#ifndef STRING_MESSAGE_WITH_PAYLOAD_LENGTH_H
#define STRING_MESSAGE_WITH_PAYLOAD_LENGTH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef struct message_with_length_t {
        int fd;
        char * content; // '\0' terminated (at content_len)
        size_t content_len; // STOMP frame bytes without the terminating '\0' (raw: all bytes)
        size_t frame_len; // bytes on the wire (raw: exact content length)
        int raw; // content is already encoded: write frame_len bytes as they are
        int close_after; // close the connection once this message is written
    } message_with_frame_len;

    message_with_frame_len * message_create_with_frame_len(int fd, char * str);
    /** STOMP frame of len bytes (the body may contain '\0'). */
    message_with_frame_len * message_create_with_frame_len_n(int fd, const char * bytes, size_t len);

    /**
     * Pre-encoded bytes (WebSocket control frames, HTTP handshake responses).
     * May contain '\0' bytes, so the length is explicit.
     */
    message_with_frame_len * message_create_raw(int fd, const char * bytes, size_t len, int close_after);
    
    void message_destroy_with_frame_len(message_with_frame_len *m);

    message_with_frame_len * message_poison_pill_with_frame_len();

#ifdef __cplusplus
}
#endif

#endif /* STRING_MESSAGE_WITH_PAYLOAD_LENGTH_H */

