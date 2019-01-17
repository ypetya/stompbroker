
#include "buffer.h"
#include "../logger.h"

buffer_item ws_buffer[WS_CONTINUATION_BUFFER];

struct ws_buffer_stat_t {
    size_t allocated_size = 0;
    size_t ws_buffer_hit = 0;
    size_t ws_buffer_miss = 0;
} ws_buffer_stat;

void ws_init_buffer() {
    debug("server: Websocket buffer size: %d connections with %d bytes data in total.\n",
            WS_CONTINUATION_BUFFER, WS_MAX_BUFFER_SIZE);
    memset(ws_buffer, 0, sizeof (ws_buffer));
}

buffer_item* ws_buffer_find(int fd) {
    for (int i = 0; i < WS_CONTINUATION_BUFFER; i++) {
        if (ws_buffer[i].fd == fd){
            ws_buffer_hit++;
            return &ws_buffer[i];
        }
    }
    ws_buffer_miss++;
    return NULL;
}

void ws_buffer_free(buffer_item * buffer) {
    if (buffer != NULL) {
        free(buffer->received);
        ws_buffer_allocated_size -= buffer->received_len;
        memset(buffer, '\0', sizeof (buffer_item));
    }
}

// TODO: performance improvement: use buffers, not real allocations 
void ws_buffer_shrink(buffer_item * buffer, size_t old_len, size_t new_len) {
    if (buffer != NULL) {

        if (new_len > 0) {
            char* p = realloc(buffer->received, new_len);
            if (p == NULL) {
                fprintf(stderr, "Out of memory!\n");
                exit(1);
            }
            buffer->received = p;
        } else {
            free(buffer->received);
            buffer->received = NULL;
        }

        buffer->received_len = new_len;

        if (new_len < old_len)
            ws_buffer_allocated_size -= old_len - new_len; // ensure -- ==> +
        else
            ws_buffer_allocated_size += new_len - old_len;
    }
}

void ws_deinit_buffer() {
    for (int i = 0; i < WS_CONTINUATION_BUFFER; i++) {
        if (ws_buffer[i].fd != 0) ws_buffer_free(&ws_buffer[i]);
    }
}

buffer_item* ws_buffer_add() {
    for (int i = 0; i < WS_CONTINUATION_BUFFER; i++) {
        if (ws_buffer[i].fd == 0) return &ws_buffer[i];
    }
    return NULL;
}
