
#include "dataframe.h"
#include <string.h>
#include "../../logger.h"
#include "../../lib/emalloc.h"
#include "../../lib/thread_safe_queue.h"
#include "../../server/data/session_storage.h"
#include "../../parse_args.h"


#include "../buffer.h"


int encode_websocket_frame(char * buffer, char** out);

size_t ws_output_filter(message *m) {
    size_t len = strlen(m->content) + 1;
    if (session_is_encoded(m->fd)) {
        m->fd = session_without_flags(m->fd);
        char* encoded_message;
        len = encode_websocket_frame(m->content, &encoded_message);
        free(m->content);
        m->content = encoded_message;
    }
    return len;
}

int encode_websocket_frame(char * buffer, char** out) {
    int skip, len = strlen(buffer) + 1, orig_len = len;
    char * encoded_message = NULL;
    char OPCODE = '\x81';
    if (len < 126) {
        len += 2;
        encoded_message = emalloc(sizeof (char)*len);
        encoded_message[0] = OPCODE;
        encoded_message[1] = orig_len;
        memcpy(encoded_message + 2, buffer, orig_len);
    } else if (len < 65536) {
        len += 4;
        encoded_message = emalloc(sizeof (char)*len);
        encoded_message[0] = OPCODE;
        encoded_message[1] = 126;
        uint16_t sz16 = htons(orig_len);
        memcpy(encoded_message + 2, &sz16, sizeof (uint16_t));
        memcpy(encoded_message + 4, buffer, orig_len);
    } else {
        len += 10;
        encoded_message = emalloc(sizeof (char)*len);
        encoded_message[0] = OPCODE;
        encoded_message[1] = 127;
        uint64_t sz64 = ntohl64(orig_len);
        memcpy(encoded_message + 2, &sz64, sizeof (uint64_t));
        memcpy(encoded_message + 10, buffer, orig_len);
    }
    int fin = (encoded_message[0] & 0x80 ? 1 : 0);
    int op_code = (encoded_message[0] & 0xF);

    debug(">>> Websocket data frame FIN: %d opcode: 0x%x payload_len: %" PRIu64 "\n", fin, op_code, orig_len);
    *out = encoded_message;
    return len;
}

size_t ws_dataframe_read_headers(buffer_item* buf);

char* ws_dataframe_decode(buffer_item* buf);

/**
 * This method concatenates buffer, when needed for specific fd
 * 
 * @param fd
 * @param buffer
 * @param out  only complete, decoded data-frames ( can contain multiple stomp messages)
 * @return status
 */
ws_filter_dataframe_status ws_input_filter_dataframe(int fd, char* buffer, size_t read_len, char** out, size_t *decoded_buf_len) {

    int has_mask = has_mask = buffer[1] & 0x80 ? 1 : 0;
    // Be aware: session_storage_is_encoded should be used on the same thread as all the session_storage_functions!
    if (has_mask || session_storage_is_encoded(fd)) {
        int new_buf = 0;
        buffer_item * ws_buff = ws_buffer_find(fd);

        ws_buffer_allocated_size += read_len;
        if (ws_buffer_allocated_size > WS_MAX_BUFFER_SIZE) {
            if (ws_buff) ws_buffer_free(ws_buff);
            else ws_buffer_allocated_size -= read_len;

            return WS_BUFFER_EXCEEDED_MAX;
        }

        // merge
        if (ws_buff != NULL) {
            // FIXME guard reallocs
            ws_buff->received = realloc(ws_buff->received, ws_buff->received_len + read_len + 1);
            memcpy(&ws_buff->received[ws_buff->received_len], buffer, read_len);
            ws_buff->received_len += read_len;
            buffer = ws_buff->received;
            ws_buff->remaining_len = ws_buff->received_len - ws_buff->frame_len;

            debug("Merged dataframes. Total: %d Buffer size: %d fd: %d\n",
                    ws_buffer_allocated_size, ws_buff->received_len, ws_buff->fd);
            //was continued
        } else {
            ws_buff = ws_buffer_add();
            ws_buff->fd = fd;
            ws_buff->received_len = read_len;
            ws_buff->received = emalloc(read_len);
            memcpy(ws_buff->received, buffer, read_len);

            // parse headers & fill buffer_item
            if (ws_dataframe_read_headers(ws_buff) <= 0)
                return WS_TOO_LARGE_DATAFRAME;


            debug("New dataframe. FrameSize: %d Total: %d Buffer size: %d fd: %d\n",
                    ws_buff->frame_len,
                    ws_buffer_allocated_size, ws_buff->received_len, ws_buff->fd);
        }
        // do pop decoded msg & update buffer_item while can
        size_t decoded_data_len = ws_buff->frame_len;
        char * decoded_data = ws_dataframe_decode(ws_buff);

        if (decoded_data != NULL) {
            char * out = ws_dataframe_decode(ws_buff);

            while (out != NULL) {
                size_t new_len = decoded_data_len + ws_buff->frame_len;
                decoded_data = realloc(decoded_data, new_len + 1);
                memcpy(&decoded_data[decoded_data_len], out, ws_buff->frame_len);
                decoded_data_len += ws_buff->frame_len;
                free(out);
                out = ws_dataframe_decode(ws_buff);
            }
        }
        // if no left, free space
        if (ws_buff->remaining_len == 0) {
            ws_buffer_free(ws_buff);
        }

        // data out, return that
        if (decoded_data != NULL) {
            *out = decoded_data;
            *decoded_buf_len = decoded_data_len;
            return WS_COMPLETE_DATAFRAME;
        }

        return WS_INCOMPLETE_DATAFRAME;
    }

    return WS_NOT_A_DATAFRAME;
}

char * ws_dataframe_decode(buffer_item* buf) {
    size_t len = buf->frame_len;
    if (len <= 0) return NULL;

    // calc the skip
    int skip = 0;
    if (len < 126) skip = 6;
    else if (len < 65536)skip = 8;
    else skip = 14;
    // decode first frame

    char * decoded_message = emalloc(buf->frame_len + 1);

    for (size_t i = 0; i < len; i++) decoded_message[i] = buf->received[i + skip] ^ buf->mask[i % 4];
    debug("Decoded frame len(%d)\n", len);
    // shrink buffer_item and set new headers
    buf->remaining_len = buf->received_len - len - skip;
    if (buf->remaining_len > 0) {
        debug("Remaining chunk len: %d\n", buf->remaining_len);
        // shrink payload, and calc new headers
        for (size_t i = 0; i < buf->remaining_len; i++) buf->received[i] = buf->received[i + skip + buf->frame_len];
        buf->received = realloc(buf->received, buf->remaining_len + 1);
        buf->received_len = buf->remaining_len;
        // calc next_frame len
        ws_dataframe_read_headers(buf);
    } else {
        buf->frame_len = 0;
    }

    return decoded_message;
}

#define WS_DATA_FRAME_MAX_LENGTH 1000000

unsigned int ws_dropped_frames = 0;
char mask[4];
size_t mask_len = sizeof (mask);

size_t ws_dataframe_read_headers(buffer_item* buf) {
    char * buffer = buf->received;

    // TODO: check fin flag and append continue
    int fin = (buffer[0] & 0x80 ? 1 : 0);
    int op_code = (buffer[0] & 0xF);

    if (op_code == 1) {
        session_storage_set_encoded(buf->fd);
    } else {
        // TODO handle PING-FRAMES
        ws_dropped_frames++;
        return buf->frame_len = 0;
    }

    uint64_t payload_len = 0;

    int len = buffer[1] & 0x7f;

    if (len < 126) {
        payload_len += len;
        memcpy(&buf->mask, buffer + 2, mask_len);
    } else if (len == 126) {
        uint16_t sz16;
        memcpy(&sz16, buffer + 2, sizeof (uint16_t));
        payload_len += ntohs(sz16);
        memcpy(&buf->mask, buffer + 4, mask_len);
    } else if (len == 127) {
        uint64_t sz64;
        memcpy(&sz64, buffer + 2, sizeof (uint64_t));
        payload_len = ntohl64(sz64);
        memcpy(&buf->mask, buffer + 10, mask_len);
    }
    debug("Parsed Websocket data-frame header FIN: %d opcode: 0x%x payload_len: %" PRIu64 "\n", fin, op_code, payload_len);
    if (payload_len > WS_DATA_FRAME_MAX_LENGTH) { // maximum message size 1M
        ws_dropped_frames++;
        return buf->frame_len = 0;
    }

    return buf->frame_len = payload_len;
}
