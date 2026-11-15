#ifndef STOMP_FRAMING_H
#define STOMP_FRAMING_H

/**
 * # Responsibility
 *
 * Cuts a STOMP byte stream into frames (STOMP 1.2 "Augmented BNF"):
 *
 * - EOLs between frames (heart-beats) are skipped,
 * - a frame ends at its first NUL, or - when it has a content-length header -
 *   at header end + content-length, where a NUL must follow (the body may
 *   contain NULs then),
 * - an incomplete frame is kept for the next read,
 * - a frame longer than max_frame_len is an error.
 *
 * Pure functions plus a per-connection buffer, so it is shared by raw TCP
 * and WebSocket input and unit testable without sockets.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum stomp_frame_status_t {
        STOMP_FRAME_NEED_MORE = 0,
        STOMP_FRAME_COMPLETE = 1,
        STOMP_FRAME_ERROR = -1
    } stomp_frame_status;

    typedef struct stomp_frame_span_t {
        size_t start; // offset of the command (leading EOLs skipped)
        size_t len; // frame length without the terminating NUL
        size_t consumed; // bytes the caller may drop: the frame and its NUL, or the skipped EOLs
        const char *error; // STOMP_FRAME_ERROR: static reason
    } stomp_frame_span;

    /** Finds the first frame in buf. */
    stomp_frame_status stomp_find_frame(const char *buf, size_t len, size_t max_frame_len,
            stomp_frame_span *span);

    /** Bytes of a connection that do not form a complete frame yet. */
    typedef struct stomp_input_buffer_t {
        char *data;
        size_t len;
    } stomp_input_buffer;

    /** Receives one complete frame (without its NUL); frame is only valid during the call. */
    typedef void (*stomp_frame_sink)(void *ctx, const char *frame, size_t len);

    /**
     * Appends received bytes and hands every complete frame to sink, in order.
     * @return 0, or -1 on a framing error: *error is set and the buffer is cleared
     */
    int stomp_input_feed(stomp_input_buffer *in, const char *data, size_t len, size_t max_frame_len,
            stomp_frame_sink sink, void *ctx, const char **error);

    void stomp_input_reset(stomp_input_buffer *in);

    /** Input buffer of a connection. Owned by the epoll thread, like the WebSocket buffers. */
    stomp_input_buffer *stomp_input_of(int fd);

#ifdef __cplusplus
}
#endif

#endif /* STOMP_FRAMING_H */
