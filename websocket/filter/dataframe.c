
#include "dataframe.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include "../../logger.h"
#include "../../lib/emalloc.h"
#include "../../server/data/session_storage.h"

#include "../buffer.h"

uint64_t ws_dropped_frames = 0;
uint64_t ws_processed_incoming_frames = 0;

/* ======================================================================
 * Single frame codec
 * ====================================================================== */

static int ws_opcode_is_known(int opcode) {
    switch (opcode) {
        case WS_OP_CONTINUATION:
        case WS_OP_TEXT:
        case WS_OP_BINARY:
        case WS_OP_CLOSE:
        case WS_OP_PING:
        case WS_OP_PONG:
            return 1;
        default:
            return 0;
    }
}

static ws_decode_status ws_decode_fail(ws_frame *frame, uint16_t close_code) {
    frame->close_code = close_code;
    frame->consumed = 0;
    return WS_DECODE_ERROR;
}

static uint64_t read_be(const unsigned char *p, int bytes) {
    uint64_t v = 0;
    for (int i = 0; i < bytes; i++) v = (v << 8) | p[i];
    return v;
}

ws_decode_status ws_decode_frame(const unsigned char *buf, size_t len,
        uint64_t max_payload, ws_frame *frame) {
    memset(frame, 0, sizeof (ws_frame));
    if (len < 2) return WS_DECODE_NEED_MORE;

    frame->fin = (buf[0] & 0x80) ? 1 : 0;
    frame->opcode = buf[0] & 0x0f;
    int reserved_bits = buf[0] & 0x70;
    int masked = (buf[1] & 0x80) ? 1 : 0;
    uint64_t payload_len = buf[1] & 0x7f;
    int is_control = frame->opcode & 0x08;

    if (reserved_bits) return ws_decode_fail(frame, WS_CLOSE_PROTOCOL_ERROR); // no extensions negotiated
    if (!ws_opcode_is_known(frame->opcode)) return ws_decode_fail(frame, WS_CLOSE_PROTOCOL_ERROR);
    if (!masked) return ws_decode_fail(frame, WS_CLOSE_PROTOCOL_ERROR); // RFC 6455 5.1: clients MUST mask
    if (is_control && (!frame->fin || payload_len > WS_CONTROL_MAX_PAYLOAD))
        return ws_decode_fail(frame, WS_CLOSE_PROTOCOL_ERROR);

    size_t header_len = 2;
    if (payload_len == 126) {
        if (len < 4) return WS_DECODE_NEED_MORE;
        payload_len = read_be(buf + 2, 2);
        header_len = 4;
    } else if (payload_len == 127) {
        if (len < 10) return WS_DECODE_NEED_MORE;
        payload_len = read_be(buf + 2, 8);
        header_len = 10;
        if (payload_len >> 63) return ws_decode_fail(frame, WS_CLOSE_PROTOCOL_ERROR);
    }

    if (payload_len > max_payload) return ws_decode_fail(frame, WS_CLOSE_MESSAGE_TOO_BIG);

    const unsigned char *mask = buf + header_len;
    header_len += 4;
    if (len < header_len || (uint64_t) (len - header_len) < payload_len) return WS_DECODE_NEED_MORE;

    frame->payload = emalloc((size_t) payload_len + 1); // zeroed: payload[payload_len] == '\0'
    for (size_t i = 0; i < payload_len; i++)
        frame->payload[i] = (char) (buf[header_len + i] ^ mask[i % 4]);
    frame->payload_len = (size_t) payload_len;
    frame->consumed = header_len + (size_t) payload_len;

    debug("Websocket frame FIN: %d opcode: 0x%x payload_len: %zu\n", frame->fin, frame->opcode, frame->payload_len);
    return WS_DECODE_OK;
}

size_t ws_frame_header_len(size_t payload_len) {
    if (payload_len < 126) return 2;
    if (payload_len < 65536) return 4;
    return 10;
}

size_t ws_encode_frame(int opcode, const char *payload, size_t len, char **out) {
    size_t header_len = ws_frame_header_len(len);
    unsigned char *frame = emalloc(header_len + len);

    frame[0] = (unsigned char) (0x80 | (opcode & 0x0f)); // FIN, no fragmentation on output
    if (header_len == 2) {
        frame[1] = (unsigned char) len;
    } else if (header_len == 4) {
        frame[1] = 126;
        frame[2] = (unsigned char) (len >> 8);
        frame[3] = (unsigned char) len;
    } else {
        frame[1] = 127;
        for (int i = 0; i < 8; i++) frame[2 + i] = (unsigned char) ((uint64_t) len >> (8 * (7 - i)));
    }
    if (len > 0) memcpy(frame + header_len, payload, len);

    *out = (char *) frame;
    return header_len + len;
}

size_t ws_encode_control(int opcode, const char *payload, size_t len, char **out) {
    if (len > WS_CONTROL_MAX_PAYLOAD) len = WS_CONTROL_MAX_PAYLOAD;
    return ws_encode_frame(opcode, payload, len, out);
}

size_t ws_encode_close(uint16_t code, char **out) {
    char status[2] = {(char) (code >> 8), (char) (code & 0xff)};
    return ws_encode_control(WS_OP_CLOSE, status, sizeof (status), out);
}

/* ======================================================================
 * Output filter
 * ====================================================================== */

/** STOMP text frame including the closing '\0' of the STOMP message. */
static size_t encode_websocket_frame(char * buffer, char** out) {
    return ws_encode_frame(WS_OP_TEXT, buffer, strlen(buffer) + 1, out);
}

size_t ws_output_filter(message_with_frame_len *m) {
    size_t len = strlen(m->content);
    if (session_is_encoded(m->fd)) {
        m->fd = session_without_flags(m->fd);
        char* encoded_message;
        len = encode_websocket_frame(m->content, &encoded_message);
        free(m->content);
        m->content = encoded_message;
    } else if (strncasecmp(m->content, "HTTP", 4) != 0) len++; // STOMP needs a closing '\0' HTTP handshake must not!
    return len;
}

/* ======================================================================
 * Per connection input filter
 * ====================================================================== */

/**
 * Either buffer contains a mask flag or the session is set to encoded.
 * This method also sets the session to encoded
 * Be aware session_storage should ne set and read from the same thread
 */
static int ws_channel_is_encoded(int fd, char* buffer, size_t len) {
    if (session_storage_is_encoded(fd)) return 1;
    if (len > 1 && (buffer[1] & 0x80)) {
        session_storage_set_encoded(fd);
        return 1;
    }
    return 0;
}

static void ws_buffer_consume(buffer_item *buf, size_t consumed) {
    size_t left = buf->received_len - consumed;
    if (consumed == 0) return;
    if (left > 0) memmove(buf->received, buf->received + consumed, left);
    ws_buffer_resize(buf, buf->received_len, left);
}

/**
 * This method concatenates buffer, when needed for specific fd
 *
 * @param fd
 * @param buffer
 * @param out  only complete, decoded data-frames ( can contain multiple stomp messages)
 * @return status
 */
ws_filter_dataframe_status ws_input_filter_dataframe(int fd, char* buffer, size_t read_len, char** out, size_t* decoded_buf_len) {
    *out = NULL;
    if (!ws_channel_is_encoded(fd, buffer, read_len)) return WS_NOT_A_DATAFRAME;

    buffer_item * ws_buff = ws_buffer_find(fd);

    if (ws_buffer_size_left(read_len) < 0) {
        if (ws_buff) ws_buffer_free(ws_buff);
        return WS_BUFFER_EXCEEDED_MAX;
    }

    if (ws_buff != NULL) {
        size_t old_len = ws_buff->received_len;
        ws_buffer_resize(ws_buff, old_len, old_len + read_len);
        memcpy(&ws_buff->received[old_len], buffer, read_len);
    } else {
        ws_buff = ws_buffer_add(fd, buffer, read_len);
        if (!ws_buff) return WS_BUFFER_OUT_OF_SLOTS;
    }

    size_t ag_decoded_data_len = 0;
    char * ag_decoded_data = NULL;
    ws_filter_dataframe_status status = WS_INCOMPLETE_DATAFRAME;
    size_t offset = 0;

    while (status == WS_INCOMPLETE_DATAFRAME) {
        ws_frame frame;
        ws_decode_status decoded = ws_decode_frame((unsigned char *) ws_buff->received + offset,
                ws_buff->received_len - offset, WS_DATA_FRAME_MAX_LENGTH, &frame);
        if (decoded == WS_DECODE_NEED_MORE) break;
        if (decoded == WS_DECODE_ERROR) {
            ws_dropped_frames++;
            status = frame.close_code == WS_CLOSE_MESSAGE_TOO_BIG ? WS_TOO_LARGE_DATAFRAME : WS_INVALID_HEADER;
            break;
        }
        offset += frame.consumed;
        ws_processed_incoming_frames++;

        if (frame.opcode == WS_OP_TEXT) {
            ag_decoded_data = erealloc(ag_decoded_data, ag_decoded_data_len + frame.payload_len);
            memcpy(&ag_decoded_data[ag_decoded_data_len], frame.payload, frame.payload_len);
            ag_decoded_data_len += frame.payload_len;
        } else if (frame.opcode == WS_OP_CLOSE) {
            status = WS_OPCODE_CLIENT_DISCONNECT;
        } else {
            ws_dropped_frames++;
            status = WS_OPCODE_UNHANDLED;
        }
        free(frame.payload);
    }

    ws_buffer_consume(ws_buff, offset);
    if (ws_buff->received == NULL) ws_buffer_free(ws_buff);

    if (ag_decoded_data_len == 0) {
        free(ag_decoded_data);
    } else {
        *out = ag_decoded_data;
        *decoded_buf_len = ag_decoded_data_len - 1;
        if (status == WS_INCOMPLETE_DATAFRAME) status = WS_COMPLETE_DATAFRAME;
    }
    return status;
}
