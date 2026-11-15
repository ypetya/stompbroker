
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


size_t ws_output_filter(message_with_frame_len *m) {
    if (m->raw) { // already encoded control frame / HTTP response
        m->fd = session_without_flags(m->fd);
        return m->frame_len;
    }
    size_t len = m->content_len;
    if (session_is_encoded(m->fd)) {
        m->fd = session_without_flags(m->fd);
        char* encoded_message;
        // text frame carrying the STOMP frame and its closing '\0'
        len = ws_encode_frame(WS_OP_TEXT, m->content, m->content_len + 1, &encoded_message);
        free(m->content);
        m->content = encoded_message;
    } else {
        len++; // raw TCP: the closing '\0' of the STOMP frame (HTTP responses are raw messages)
    }
    return len;
}

/* ======================================================================
 * Per connection input filter
 * ====================================================================== */

static void append_bytes(char **buf, size_t *len, const char *data, size_t n) {
    if (n == 0) return;
    *buf = erealloc(*buf, *len + n);
    memcpy(*buf + *len, data, n);
    *len += n;
}

/** STOMP heart-beats are bare EOLs; they carry no frame for the STOMP layer. */
static int is_heartbeat(const char *payload, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (payload[i] != '\n' && payload[i] != '\r') return 0;
    return 1;
}

/** Adds one complete application message, always '\0' terminated. */
static void deliver_message(ws_input_result *result, const char *payload, size_t len) {
    if (is_heartbeat(payload, len)) return;
    append_bytes(&result->messages, &result->messages_len, payload, len);
    if (payload[len - 1] != '\0') append_bytes(&result->messages, &result->messages_len, "", 1);
}

static void reply_frame(ws_input_result *result, char *frame, size_t len) {
    append_bytes(&result->replies, &result->replies_len, frame, len);
    free(frame);
}

static void reply_close(ws_input_result *result, uint16_t code) {
    char *frame = NULL;
    size_t len = ws_encode_close(code, &frame);
    reply_frame(result, frame, len);
    result->close = 1;
    result->close_code = code;
}

/** Echoes the client's close frame (status code only) and requests closing. */
static void reply_close_echo(ws_input_result *result, const ws_frame *frame) {
    char *echo = NULL;
    size_t echo_len = ws_encode_control(WS_OP_CLOSE, frame->payload, frame->payload_len >= 2 ? 2 : 0, &echo);
    reply_frame(result, echo, echo_len);
    result->close = 1;
    result->close_code = frame->payload_len >= 2
            ? (uint16_t) (((unsigned char) frame->payload[0] << 8) | (unsigned char) frame->payload[1])
            : WS_CLOSE_NORMAL;
}

static ws_filter_dataframe_status protocol_error(ws_input_result *result) {
    reply_close(result, WS_CLOSE_PROTOCOL_ERROR);
    return WS_INVALID_HEADER;
}

/** First frame of a message (text or binary). */
static ws_filter_dataframe_status ws_start_message(int fd, ws_frame *frame, ws_input_result *result) {
    buffer_item *slot = ws_buffer_find(fd);
    if (slot != NULL && slot->in_fragment) return protocol_error(result); // previous message unfinished

    if (frame->fin) {
        deliver_message(result, frame->payload, frame->payload_len);
        return WS_INCOMPLETE_DATAFRAME;
    }

    slot = ws_buffer_get_or_add(fd);
    if (slot == NULL) {
        reply_close(result, WS_CLOSE_INTERNAL_ERROR);
        return WS_BUFFER_OUT_OF_SLOTS;
    }
    ws_fragment_append(slot, frame->payload, frame->payload_len);
    return WS_INCOMPLETE_DATAFRAME;
}

/** Continuation frame: reassemble, deliver on FIN, enforce the message size cap. */
static ws_filter_dataframe_status ws_continue_message(int fd, ws_frame *frame,
        size_t max_message_len, ws_input_result *result) {
    buffer_item *slot = ws_buffer_find(fd);
    if (slot == NULL || !slot->in_fragment) return protocol_error(result); // nothing to continue

    if (slot->fragments_len + frame->payload_len > max_message_len) {
        reply_close(result, WS_CLOSE_MESSAGE_TOO_BIG);
        return WS_TOO_LARGE_DATAFRAME;
    }
    ws_fragment_append(slot, frame->payload, frame->payload_len);

    if (frame->fin) {
        deliver_message(result, slot->fragments, slot->fragments_len);
        ws_fragment_clear(slot);
    }
    return WS_INCOMPLETE_DATAFRAME;
}

/**
 * Applies one decoded frame to the connection. Control frames may arrive
 * between the fragments of a message.
 * @return status; WS_INCOMPLETE_DATAFRAME means "keep going"
 */
static ws_filter_dataframe_status ws_dispatch_frame(int fd, ws_frame *frame,
        size_t max_message_len, ws_input_result *result) {
    switch (frame->opcode) {
        case WS_OP_TEXT:
        case WS_OP_BINARY: // policy: binary payload is treated as STOMP bytes, same as text
            return ws_start_message(fd, frame, result);
        case WS_OP_CONTINUATION:
            return ws_continue_message(fd, frame, max_message_len, result);
        case WS_OP_PING:
        {
            char *pong = NULL;
            size_t pong_len = ws_encode_control(WS_OP_PONG, frame->payload, frame->payload_len, &pong);
            reply_frame(result, pong, pong_len);
            return WS_INCOMPLETE_DATAFRAME;
        }
        case WS_OP_PONG:
            return WS_INCOMPLETE_DATAFRAME; // unsolicited pong: ignore
        case WS_OP_CLOSE:
            if (frame->payload_len == 1) return protocol_error(result); // a status code needs 2 bytes
            reply_close_echo(result, frame);
            return WS_OPCODE_CLIENT_DISCONNECT;
        default: // ws_decode_frame rejects unknown opcodes already
            return protocol_error(result);
    }
}

/**
 * Decodes and dispatches every complete frame in src.
 * @param consumed out: bytes used from src
 */
static ws_filter_dataframe_status ws_process_frames(int fd, const char *src, size_t len,
        size_t max_message_len, ws_input_result *result, size_t *consumed) {
    ws_filter_dataframe_status status = WS_INCOMPLETE_DATAFRAME;
    size_t offset = 0;

    while (status == WS_INCOMPLETE_DATAFRAME) {
        ws_frame frame;
        ws_decode_status decoded = ws_decode_frame((const unsigned char *) src + offset,
                len - offset, max_message_len, &frame);
        if (decoded == WS_DECODE_NEED_MORE) break;
        if (decoded == WS_DECODE_ERROR) {
            ws_dropped_frames++;
            reply_close(result, frame.close_code);
            status = frame.close_code == WS_CLOSE_MESSAGE_TOO_BIG ? WS_TOO_LARGE_DATAFRAME : WS_INVALID_HEADER;
            break;
        }
        offset += frame.consumed;
        ws_processed_incoming_frames++;
        status = ws_dispatch_frame(fd, &frame, max_message_len, result);
        free(frame.payload);
    }

    *consumed = offset;
    return status;
}

ws_filter_dataframe_status ws_input_filter_dataframe(int fd, const char *data, size_t len,
        size_t max_message_len, ws_input_result *result) {
    memset(result, 0, sizeof (ws_input_result));

    buffer_item *slot = ws_buffer_find(fd);
    int buffered = slot != NULL && slot->received_len > 0;

    if (ws_buffer_size_left(len) < 0) {
        ws_buffer_free(slot);
        reply_close(result, WS_CLOSE_INTERNAL_ERROR);
        return WS_BUFFER_EXCEEDED_MAX;
    }

    // Decode straight from the read buffer unless earlier bytes are pending.
    if (buffered) {
        size_t old_len = slot->received_len;
        ws_buffer_resize(slot, old_len, old_len + len);
        memcpy(slot->received + old_len, data, len);
        data = slot->received;
        len = slot->received_len;
    }

    size_t consumed = 0;
    ws_filter_dataframe_status status = ws_process_frames(fd, data, len, max_message_len, result, &consumed);
    slot = ws_buffer_find(fd); // dispatch may have created a slot for fragments

    if (result->close) {
        ws_buffer_free(slot); // nothing more will be read from this connection
    } else if (buffered) {
        size_t left = slot->received_len - consumed;
        if (consumed > 0 && left > 0) memmove(slot->received, slot->received + consumed, left);
        ws_buffer_resize(slot, slot->received_len, left);
    } else if (consumed < len) { // keep the partial frame for the next read
        slot = ws_buffer_get_or_add(fd);
        if (slot == NULL) {
            reply_close(result, WS_CLOSE_INTERNAL_ERROR);
            return WS_BUFFER_OUT_OF_SLOTS;
        }
        ws_buffer_resize(slot, 0, len - consumed);
        memcpy(slot->received, data + consumed, len - consumed);
    }

    if (slot != NULL && !result->close && ws_buffer_is_idle(slot)) ws_buffer_free(slot);

    if (status == WS_INCOMPLETE_DATAFRAME && result->messages_len > 0) status = WS_COMPLETE_DATAFRAME;
    return status;
}

void ws_input_result_free(ws_input_result *result) {
    free(result->messages);
    free(result->replies);
    memset(result, 0, sizeof (ws_input_result));
}
