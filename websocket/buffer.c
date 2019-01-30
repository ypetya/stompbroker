
#include "buffer.h"
#include "../logger.h"

buffer_item ws_buffer[WS_CONTINUATION_BUFFER];

struct ws_buffer_stat_t ws_buffer_stat = {0};

void ws_init_buffer() {
    debug("server: Websocket buffer size: %d connections with %d bytes data in total.\n",
            WS_CONTINUATION_BUFFER, WS_MAX_BUFFER_SIZE);
    memset(ws_buffer, 0, sizeof (ws_buffer));
}

buffer_item* ws_buffer_find(int fd) {
    for (int i = 0; i < WS_CONTINUATION_BUFFER; i++) {
        if (ws_buffer[i].fd == fd){
            ws_buffer_stat.hit++;
            return &ws_buffer[i];
        }
    }
    ws_buffer_stat.miss++;
    return NULL;
}

void ws_buffer_free(buffer_item * buffer) {
    if (buffer != NULL) {
        free(buffer->received);
        ws_buffer_stat.allocated_size -= buffer->received_len;
        memset(buffer, '\0', sizeof (buffer_item));
    }
}

// TODO: performance improvement: use buffers, not real allocations 
void ws_buffer_resize(buffer_item * buffer, size_t old_len, size_t new_len) {
    if (buffer != NULL) {

        if (new_len > 0) {
            buffer->received = erealloc(buffer->received, new_len);
        } else {
            free(buffer->received);
            buffer->received = NULL;
        }

        buffer->received_len = new_len;

        if (new_len < old_len)
            ws_buffer_stat.allocated_size -= old_len - new_len; // ensure -- ==> +
        else
            ws_buffer_stat.allocated_size += new_len - old_len;
    }
}

void ws_deinit_buffer() {
    for (int i = 0; i < WS_CONTINUATION_BUFFER; i++) {
        if (ws_buffer[i].fd != 0) ws_buffer_free(&ws_buffer[i]);
    }
}

int ws_buffer_size_left(size_t size_left) {
    return WS_MAX_BUFFER_SIZE - (ws_buffer_stat.allocated_size + size_left);
}

buffer_item* ws_buffer_add(int fd, char*buffer,size_t read_len) {
    buffer_item* ws_buff = NULL;
    for (int i = 0; i < WS_CONTINUATION_BUFFER; i++) {
        if (ws_buffer[i].fd == 0){
            ws_buff= &ws_buffer[i];
            break;
        }
    }
    if(!ws_buff) return NULL;

    ws_buff->fd = fd;
    ws_buff->received_len = read_len;
    ws_buff->received = emalloc(read_len);
    ws_buffer_stat.allocated_size += read_len;
    memcpy(ws_buff->received, buffer, read_len);
    
    return ws_buff;
}

struct ws_buffer_stat_t * ws_buffer_get_stats() {
    return &ws_buffer_stat;
}