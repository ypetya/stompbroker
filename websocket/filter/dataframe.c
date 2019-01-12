
#include "dataframe.h"
#include <string.h>
#include "../../logger.h"
#include "../../lib/emalloc.h"
#include "../../lib/thread_safe_queue.h"
#include "../../server/data/session_storage.h"
#include "../../parse_args.h"


#include "../buffer.h"

uint64_t ws_dropped_frames = 0;
uint64_t ws_processed_incoming_frames = 0;
char mask[4];
size_t mask_len = sizeof (mask);

int encode_websocket_frame(char * buffer, char** out);

size_t ws_output_filter(message_with_frame_len *m) {
    size_t len = strlen(m->content);
    if (session_is_encoded(m->fd)) {
        m->fd = session_without_flags(m->fd);
        char* encoded_message;
        len = encode_websocket_frame(m->content, &encoded_message);
        free(m->content);
        m->content = encoded_message;
        #ifdef DEBUG
            printf("fd: %d, data:",m->fd);
            for(int i=0;i<len;i++) printf("%02x",m->content[i] & 0xff);
            printf("\n\n");
        #endif
    } else if(strncasecmp(m->content,"HTTP",4)!=0) len++; // STOMP needs a closing '\0' HTTP handshake must not!
    return len;
}

int encode_websocket_frame(char * buffer, char** out) {
    size_t len = strlen(buffer)+1;
    size_t orig_len = len;
    char * encoded_message = NULL;
    char OPCODE = '\x81';
    if (orig_len < 126) {
        len += 2;
        encoded_message = emalloc(sizeof (char)*len);
        encoded_message[0] = OPCODE;
        encoded_message[1] = (orig_len) & 0x7f;
        memcpy(encoded_message + 2, buffer, orig_len);
    } else if (orig_len < 65536) {
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

    debug(">>> Websocket data frame FIN: %d opcode: 0x%x payload_len: %llu\n", fin, op_code, orig_len);
    *out = encoded_message;
    return len;
}

size_t ws_dataframe_read_headers(buffer_item* buf);

char* ws_dataframe_decode(buffer_item* buf);

/**
 * Either buffer contains a mask flag or the session is set to encoded.
 * This method also sets the session to encoded
 * Be aware session_storage should ne set and read from the same thread
*/
int ws_channel_is_encoded(int fd, char* buffer) {
    int is_data_frame = 0;
    int has_mask = has_mask = buffer[1] & 0x80 ? 1 : 0;
    if(!has_mask) {
        is_data_frame = session_storage_is_encoded(fd);
    } 
    if(has_mask) {
        if(!is_data_frame) session_storage_set_encoded(fd);
        is_data_frame = 1;
    }
    return is_data_frame;
}

/**
 * This method concatenates buffer, when needed for specific fd
 * 
 * @param fd
 * @param buffer
 * @param out  only complete, decoded data-frames ( can contain multiple stomp messages)
 * @return status
 */
ws_filter_dataframe_status ws_input_filter_dataframe(int fd, char* buffer, size_t read_len, char** out, size_t *decoded_buf_len) {
    *out = NULL;
    if (ws_channel_is_encoded(fd, buffer)) {
        buffer_item * ws_buff = ws_buffer_find(fd);

        if ( (ws_buffer_allocated_size + read_len) > WS_MAX_BUFFER_SIZE) {
            if (ws_buff) ws_buffer_free(ws_buff);
            return WS_BUFFER_EXCEEDED_MAX;
        }

        // merge
        if (ws_buff != NULL) {
            int old_len = ws_buff->received_len;
            ws_buffer_shrink(ws_buff,old_len, ws_buff->received_len + read_len );
            memcpy(&ws_buff->received[old_len], buffer, read_len);
            ws_buff->frame_len=0;
            //ws_buff->remaining_len=0;
            debug("Merged dataframes. Total allocation: %d Buffer size: %d fd: %d\n",
                    ws_buffer_allocated_size, ws_buff->received_len, ws_buff->fd);
            //was continued
        } else {
            ws_buff = ws_buffer_add();
            ws_buff->fd = fd;
            ws_buff->received_len = read_len;
            ws_buff->received = emalloc(read_len);
            ws_buffer_allocated_size += read_len;
            memcpy(ws_buff->received, buffer, read_len);

            debug("New dataframe. Total allocation: %d Buffer size: %d fd: %d\n",
                    ws_buffer_allocated_size, 
                    ws_buff->received_len, 
                    ws_buff->fd);
        }

        size_t ag_decoded_data_len = 0;
        char * ag_decoded_data = NULL;

        // 7-> minimum frame size
        while(ws_buff->received_len>16 &&( ws_buff->frame_len==0)) {
            
            size_t full_frame_len = ws_dataframe_read_headers(ws_buff);
            if (full_frame_len < 0)
                return WS_TOO_LARGE_DATAFRAME;
            if (full_frame_len == 0)
                return WS_INVALID_HEADER;
            
            if(ws_buff->received_len < full_frame_len) {
                break;
            } else {
                // we can decode one
                size_t decoded_data_len = ws_buff->frame_len;
                char * decoded_data = ws_dataframe_decode(ws_buff);

                ag_decoded_data = realloc(ag_decoded_data, ag_decoded_data_len + decoded_data_len);
                memcpy(&ag_decoded_data[ag_decoded_data_len], decoded_data, decoded_data_len);
                ag_decoded_data_len += decoded_data_len;

                free(decoded_data);
                ws_processed_incoming_frames++;
            }
        }

        // if no left, free space
        if (ws_buff->received == NULL) {
            ws_buffer_free(ws_buff);
        }

        // data out, return that
        if (ag_decoded_data != NULL) {
            *out = ag_decoded_data;
            *decoded_buf_len = ag_decoded_data_len-1;
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

    char * decoded_message = emalloc(buf->frame_len);

    for (size_t i = 0; i < len; i++) decoded_message[i] = buf->received[i + skip] ^ buf->mask[i % 4];
    debug("Decoded frame len(%d)\n", len);
    // shrink buffer_item and set new headers
    int new_len = buf->received_len - len - skip;
    if (new_len > 0) {
        debug("Remaining buffer len: %d\n", new_len);
        // shrink payload, and calc new headers
        for (size_t i = 0; i < new_len; i++) 
            buf->received[i] = buf->received[skip + len + i];
        ws_buffer_shrink(buf, buf->received_len, new_len);
    } else {
        ws_buffer_shrink(buf,buf->received_len,0);
    }
    
    buf->frame_len = 0;
    
    return decoded_message;
}

#define WS_DATA_FRAME_MAX_LENGTH 1000000


/**
 * sets buf's mask and frame_len attribute, which willcontain the payload_length
 * @returns full frame length = (header+payload)
*/
size_t ws_dataframe_read_headers(buffer_item* buf) {
    char * buffer = buf->received;

    // TODO: check fin flag and append continue
    int fin = (buffer[0] & 0x80 ? 1 : 0);
    int op_code = (buffer[0] & 0xF);

    if (op_code == 1) {
        debug("Opcode: 1\n");
    } else {
        //debug("Unhandled opcode: %d\n", op_code);
        warn("Invalid ws fin: %d opcode: %d\n", fin, op_code);
        // TODO handle PING-FRAMES
        ws_dropped_frames++;
        return buf->frame_len = 0;
    }

    uint64_t payload_len = 0;

    int len = buffer[1] & 0x7f;
    int header_len =0;
    if (len < 126) {
        payload_len += len;
        header_len=6;
        memcpy(&buf->mask, buffer + 2, mask_len);
    } else if (len == 126) {
        uint16_t sz16;
        header_len=8;
        memcpy(&sz16, buffer + 2, sizeof (uint16_t));
        payload_len += ntohs(sz16);
        memcpy(&buf->mask, buffer + 4, mask_len);
    } else if (len == 127) {
        uint64_t sz64;
        header_len=14;
        memcpy(&sz64, buffer + 2, sizeof (uint64_t));
        payload_len = ntohl64(sz64);
        memcpy(&buf->mask, buffer + 10, mask_len);
    }
    debug("Parsed Websocket data-frame header FIN: %d opcode: 0x%x payload_len: %" PRIu64 "\n", fin, op_code, payload_len);
    if (payload_len > WS_DATA_FRAME_MAX_LENGTH) { // maximum message size 1M
        ws_dropped_frames++;
        buf->frame_len = 0;
        return WS_TOO_LARGE_DATAFRAME;
    }

    buf->frame_len = payload_len;

    return payload_len + header_len;
}
