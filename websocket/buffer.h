/* 
 * File:   buffer.h
 * Author: peter
 *
 * Created on December 30, 2018, 9:11 PM
 *
 * Per connection transport state and WebSocket input buffers.
 *
 * Ownership: everything here is read and written by the epoll (accept)
 * thread only. Other threads must not touch it; a connection closed by a
 * writer thread is reset by the epoll thread when its fd is accepted again.
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    /** Transport state of a connection (indexed by fd). */
    typedef enum ws_conn_state_t {
        CONN_NEW = 0, // nothing received yet
        CONN_RAW_STOMP, // plain STOMP over TCP
        CONN_HTTP_PENDING, // HTTP upgrade request being received
        CONN_WS_OPEN, // handshake done, WebSocket frames expected
        CONN_WS_CLOSING // close sent/received: input is ignored until the fd is closed
    } ws_conn_state;

    /** Highest fd tracked (matches MAX_NUMBER_OF_CONNECTIONS of the session storage). */
#define WS_MAX_FD 0x20000

    ws_conn_state ws_conn_get_state(int fd);
    void ws_conn_set_state(int fd, ws_conn_state state);
    /** Back to CONN_NEW, releasing any buffered input of the fd. */
    void ws_conn_reset(int fd);

    /** Buffered input of a connection that is not fully processed yet. */
    typedef struct buffer_item_t {
        int fd;
        size_t received_len;
        char* received; // undecoded bytes: partial frame or partial HTTP request
        int in_fragment; // a fragmented message (FIN=0) is being reassembled
        size_t fragments_len;
        char* fragments; // payload of the fragments received so far
    } buffer_item;

#define WS_CONTINUATION_BUFFER 1000
#define WS_MAX_BUFFER_SIZE 10000000


    void ws_init_buffer();
    void ws_deinit_buffer();

    /** @return remaining global budget if size_left more bytes were allocated (negative: exceeded) */
    long long ws_buffer_size_left(size_t size_left);

    buffer_item* ws_buffer_find(int fd);
    void ws_buffer_free(buffer_item * buffer);
    void ws_buffer_resize(buffer_item * buffer, size_t old_len, size_t new_len);
    buffer_item* ws_buffer_add(int fd, const char *buffer, size_t len);
    /** Existing slot of fd or a new, empty one (NULL when out of slots). */
    buffer_item* ws_buffer_get_or_add(int fd);
    /** True when the slot holds neither pending bytes nor a partial message. */
    int ws_buffer_is_idle(const buffer_item * buffer);

    void ws_fragment_append(buffer_item * buffer, const char *data, size_t len);
    void ws_fragment_clear(buffer_item * buffer);

    struct ws_buffer_stat_t {
        size_t allocated_size;
        size_t hit;
        size_t miss;
    };

    struct ws_buffer_stat_t * ws_buffer_get_stats();

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_H */
