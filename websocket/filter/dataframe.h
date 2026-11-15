/*
 * File:   dataframe.h
 * Author: peter
 *
 * Created on December 30, 2018, 9:15 PM
 *
 * RFC 6455 data framing: a pure single-frame codec plus the per-connection
 * input filter that buffers bytes and turns frames into STOMP messages.
 */

#ifndef DATAFRAME_H
#define DATAFRAME_H

#include <stddef.h>
#include <stdint.h>

#include "../../server/data/message/with_payload_length.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum ws_opcode_t {
        WS_OP_CONTINUATION = 0x0,
        WS_OP_TEXT = 0x1,
        WS_OP_BINARY = 0x2,
        WS_OP_CLOSE = 0x8,
        WS_OP_PING = 0x9,
        WS_OP_PONG = 0xA
    } ws_opcode;

    typedef enum ws_close_code_t {
        WS_CLOSE_NORMAL = 1000,
        WS_CLOSE_PROTOCOL_ERROR = 1002,
        WS_CLOSE_UNSUPPORTED_DATA = 1003,
        WS_CLOSE_MESSAGE_TOO_BIG = 1009,
        WS_CLOSE_INTERNAL_ERROR = 1011
    } ws_close_code;

    /* Largest payload of a single frame / reassembled message (1M). */
#define WS_DATA_FRAME_MAX_LENGTH 1000000
    /* Control frames (close, ping, pong) carry at most 125 bytes of payload. */
#define WS_CONTROL_MAX_PAYLOAD 125

    /* ------------------------------------------------------------------
     * Single frame codec (pure, no connection state)
     * ------------------------------------------------------------------ */

    typedef enum ws_decode_status_t {
        WS_DECODE_OK = 0, // one frame decoded, frame->consumed bytes used
        WS_DECODE_NEED_MORE = 1, // header or payload incomplete, nothing consumed
        WS_DECODE_ERROR = -1 // protocol violation, frame->close_code says why
    } ws_decode_status;

    typedef struct ws_frame_t {
        int fin;
        int opcode;
        char *payload; // unmasked, heap allocated, always '\0' terminated
        size_t payload_len; // without the extra '\0'
        size_t consumed; // header + payload bytes taken from the input
        uint16_t close_code; // set on WS_DECODE_ERROR
    } ws_frame;

    /**
     * Decodes the client frame at the head of buf. Header problems are
     * reported as soon as the bytes that prove them are available, so random
     * input always either makes progress or produces an error.
     *
     * Rejected with 1002: reserved bits set, unknown opcode, unmasked frame,
     * fragmented or oversized (>125) control frame, 64 bit length with MSB set.
     * Rejected with 1009: payload larger than max_payload.
     *
     * On WS_DECODE_OK the caller owns frame->payload (free()).
     */
    ws_decode_status ws_decode_frame(const unsigned char *buf, size_t len,
            uint64_t max_payload, ws_frame *frame);

    /** Header size of an unmasked (server) frame with the given payload length. */
    size_t ws_frame_header_len(size_t payload_len);

    /**
     * Encodes a single, unfragmented, unmasked server frame (FIN=1).
     * @return encoded size; *out is heap allocated
     */
    size_t ws_encode_frame(int opcode, const char *payload, size_t len, char **out);

    /** Control frame (ping/pong/close); payload is truncated to 125 bytes. */
    size_t ws_encode_control(int opcode, const char *payload, size_t len, char **out);

    /** Close frame carrying the given status code. */
    size_t ws_encode_close(uint16_t code, char **out);

    /* ------------------------------------------------------------------
     * Per connection input filter
     * ------------------------------------------------------------------ */

    typedef enum ws_filter_dataframe_status_t {
        WS_NOT_A_DATAFRAME = 0,
        WS_COMPLETE_DATAFRAME = 1, // out parameter contains at least one full decoded frame
        WS_INCOMPLETE_DATAFRAME = 2, // out parameter does not contain decoded data
        WS_TOO_LARGE_DATAFRAME = -1,
        WS_BUFFER_EXCEEDED_MAX = -2, // when no more allocation is enabled in buffer
        WS_BUFFER_OUT_OF_SLOTS = -6, // when no more continuation slots available in buffer
        WS_INVALID_HEADER = -3,
        WS_OPCODE_CLIENT_DISCONNECT = -4,
        WS_OPCODE_UNHANDLED = -5
    } ws_filter_dataframe_status;


    ws_filter_dataframe_status ws_input_filter_dataframe(int fd, char* buffer, size_t read_len, char** out, size_t *decoded_buf_len);

    /**
     * In case of an encoded session is present, encodes data into ws frames data
     *
     * @param m
     * @return message->content size
     */
    size_t ws_output_filter(message_with_frame_len *m);

#ifdef __cplusplus
}
#endif

#endif /* DATAFRAME_H */
