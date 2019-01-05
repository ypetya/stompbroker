
#include "buffer.h"
#include "../logger.h"

buffer_item ws_buffer[WS_CONTINUATION_BUFFER];

size_t ws_buffer_allocated_size = 0;

void ws_init_buffer() {
    debug("server: Websocket buffer size: %d connections with %d bytes data in total.\n",
            WS_CONTINUATION_BUFFER, WS_MAX_BUFFER_SIZE);
    memset(ws_buffer, 0, sizeof (ws_buffer));
}

buffer_item* ws_buffer_find(int fd) {
    for (int i = 0; i < WS_CONTINUATION_BUFFER; i++) {
        if (ws_buffer[i].fd == fd) return &ws_buffer[i];
    }
    return NULL;
}

void ws_buffer_free(buffer_item * buffer) {
    if (buffer != NULL) {
        free(buffer->received);
        ws_buffer_allocated_size -= buffer->received_len;
        memset(buffer, '\0', sizeof (buffer_item));
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
