/*
 * WebSocket transport unit tests (RFC 6455).
 *
 * Portable: links only the WebSocket modules and their small dependencies,
 * so it runs on Linux and macOS. Expects lib/emalloc.c and lib/clone_str.c
 * to be included by the including translation unit (test.c or
 * test_websocket_main.c), following the project's unity-build style.
 *
 * Entry point: int run_websocket_tests(void) -> number of failed tests.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lib/general_list.c"
#include "lib/queue.c"
#include "lib/thread_safe_queue.c"
#include "server/data/session_storage_simple.c"
#include "server/data/message/with_timestamp.c"
#include "server/data/message/with_payload_length.c"
#include "websocket/main.h"
#include "server/threads/output_batch.c"
#include "server/threads/output_queue.c"

#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

/* ---------------------------------------------------------------------------
 * Minimal non-aborting test framework: every failed check is reported, so a
 * single run shows the full list of broken behaviours.
 * ------------------------------------------------------------------------- */
static int ws_checks_failed = 0;
static int ws_tests_run = 0;
static int ws_tests_failed = 0;
static const char *ws_current_test = "";

#define WS_CHECK(cond) do { \
    if (!(cond)) { \
        ws_checks_failed++; \
        printf("    check failed (%s:%d) %s: %s\n", __FILE__, __LINE__, ws_current_test, #cond); \
    } \
} while (0)

#define WS_RUN(fn) do { \
    int failed_before = ws_checks_failed; \
    ws_current_test = #fn; \
    fn(); \
    ws_tests_run++; \
    if (ws_checks_failed != failed_before) ws_tests_failed++; \
    printf("  %s %s\n", ws_checks_failed == failed_before ? "PASS" : "FAIL", #fn); \
} while (0)

/* ---------------------------------------------------------------------------
 * Frame builders
 * ------------------------------------------------------------------------- */
static const unsigned char TEST_MASK[4] = {0x37, 0xfa, 0x21, 0x3d};

/** Builds a client frame into out (caller provides enough space). Returns frame size. */
static size_t make_frame(unsigned char *out, int fin, int opcode,
        const char *payload, size_t len, int masked) {
    size_t p = 0;
    out[p++] = (unsigned char) ((fin ? 0x80 : 0x00) | (opcode & 0x0f));
    unsigned char mask_bit = masked ? 0x80 : 0x00;
    if (len < 126) {
        out[p++] = mask_bit | (unsigned char) len;
    } else if (len < 65536) {
        out[p++] = mask_bit | 126;
        out[p++] = (unsigned char) (len >> 8);
        out[p++] = (unsigned char) (len & 0xff);
    } else {
        out[p++] = mask_bit | 127;
        for (int i = 7; i >= 0; i--) out[p++] = (unsigned char) ((uint64_t) len >> (8 * i));
    }
    if (masked) {
        memcpy(out + p, TEST_MASK, 4);
        p += 4;
    }
    for (size_t i = 0; i < len; i++)
        out[p + i] = (unsigned char) payload[i] ^ (masked ? TEST_MASK[i % 4] : 0);
    return p + len;
}

static char *repeat_char(char c, size_t len) {
    char *s = malloc(len + 1);
    memset(s, c, len);
    s[len] = '\0';
    return s;
}

/* A STOMP frame as sent by stomp.js: command, blank line, NUL terminator. */
static const char SMALL_STOMP[] = "SEND\n\n"; /* + the implicit '\0' -> 7 bytes */
#define SMALL_STOMP_LEN 7

static int next_fd = 100;

static int new_test_fd(void) {
    int fd = next_fd++;
    session_storage_add_new(fd);
    session_storage_set_encoded(fd); /* as after a completed handshake */
    return fd;
}

/* ---------------------------------------------------------------------------
 * Connection input filter (gaps 1-4)
 * ------------------------------------------------------------------------- */
static int feed(int fd, const void *data, size_t len, ws_input_result *r) {
    return ws_input_filter_dataframe(fd, (const char *) data, len, WS_DATA_FRAME_MAX_LENGTH, r);
}

/** Bytes of the connection's slot still waiting for more input. */
static size_t pending_bytes(int fd) {
    buffer_item *slot = ws_buffer_find(fd);
    return slot ? slot->received_len : 0;
}

static int messages_equal(const ws_input_result *r, const char *expected, size_t expected_len) {
    return r->messages != NULL && r->messages_len == expected_len && memcmp(r->messages, expected, expected_len) == 0;
}

static void test_decode_small_frame(void) {
    unsigned char frame[64];
    size_t n = make_frame(frame, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    WS_CHECK(n == 13);

    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, frame, n, &r) == WS_COMPLETE_DATAFRAME);
    WS_CHECK(messages_equal(&r, "SEND\n\n", 7));
    WS_CHECK(r.replies_len == 0 && !r.close && pending_bytes(fd) == 0);
    ws_input_result_free(&r);
}

static void test_decode_byte_by_byte(void) {
    char *body = repeat_char('x', 200);
    unsigned char frame[512];
    size_t n = make_frame(frame, 1, 1, body, 201, 1); /* include body's NUL */

    int fd = new_test_fd();
    int completes = 0;
    for (size_t i = 0; i < n; i++) {
        ws_input_result r;
        if (feed(fd, &frame[i], 1, &r) == WS_COMPLETE_DATAFRAME) {
            completes++;
            WS_CHECK(i == n - 1);
            WS_CHECK(messages_equal(&r, body, 201));
        }
        WS_CHECK(!r.close);
        ws_input_result_free(&r);
    }
    WS_CHECK(completes == 1 && pending_bytes(fd) == 0);
    free(body);
}

static void test_decode_small_frame_split_at_every_boundary(void) {
    unsigned char frame[64];
    size_t n = make_frame(frame, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    for (size_t k = 1; k < n; k++) {
        int fd = new_test_fd();
        ws_input_result r;
        WS_CHECK(feed(fd, frame, k, &r) == WS_INCOMPLETE_DATAFRAME && r.messages == NULL);
        WS_CHECK(pending_bytes(fd) == k);
        ws_input_result_free(&r);
        WS_CHECK(feed(fd, frame + k, n - k, &r) == WS_COMPLETE_DATAFRAME);
        WS_CHECK(messages_equal(&r, "SEND\n\n", 7));
        WS_CHECK(pending_bytes(fd) == 0);
        ws_input_result_free(&r);
    }
}

static void test_decode_two_small_frames_in_one_read(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    n += make_frame(buf + n, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);

    int fd = new_test_fd();
    ws_input_result r;
    feed(fd, buf, n, &r);
    WS_CHECK(messages_equal(&r, "SEND\n\n\0SEND\n\n", 14));
    ws_input_result_free(&r);
}

static void test_decode_frames_and_partial_tail(void) {
    unsigned char buf[64];
    size_t one = make_frame(buf, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    size_t n = one + make_frame(buf + one, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);

    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, buf, one + 3, &r) == WS_COMPLETE_DATAFRAME); /* one frame + 3 bytes of the next */
    WS_CHECK(messages_equal(&r, "SEND\n\n", 7) && pending_bytes(fd) == 3);
    ws_input_result_free(&r);
    WS_CHECK(feed(fd, buf + one + 3, n - one - 3, &r) == WS_COMPLETE_DATAFRAME);
    WS_CHECK(messages_equal(&r, "SEND\n\n", 7) && pending_bytes(fd) == 0);
    ws_input_result_free(&r);
}

static void test_decode_16bit_length(void) {
    char *body = repeat_char('m', 300);
    unsigned char frame[400];
    size_t n = make_frame(frame, 1, 1, body, 301, 1);

    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, frame, n, &r) == WS_COMPLETE_DATAFRAME);
    WS_CHECK(messages_equal(&r, body, 301));
    ws_input_result_free(&r);
    free(body);
}

static void test_decode_64bit_length(void) {
    size_t len = 70000;
    char *body = repeat_char('L', len);
    unsigned char *frame = malloc(len + 32);
    size_t n = make_frame(frame, 1, 1, body, len + 1, 1);

    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, frame, n, &r) == WS_COMPLETE_DATAFRAME);
    WS_CHECK(messages_equal(&r, body, len + 1));
    ws_input_result_free(&r);
    free(frame);
    free(body);
}

static void test_message_without_nul_is_terminated(void) {
    unsigned char frame[64];
    size_t n = make_frame(frame, 1, 1, "SEND\n\n", 6, 1);
    int fd = new_test_fd();
    ws_input_result r;
    feed(fd, frame, n, &r);
    WS_CHECK(messages_equal(&r, "SEND\n\n", 7));
    ws_input_result_free(&r);
}

static void test_heartbeat_is_not_a_message(void) {
    unsigned char frame[64];
    size_t n = make_frame(frame, 1, 1, "\n", 1, 1);
    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, frame, n, &r) == WS_INCOMPLETE_DATAFRAME);
    WS_CHECK(r.messages == NULL && !r.close && pending_bytes(fd) == 0);
    ws_input_result_free(&r);
}

static void test_binary_frame_policy(void) {
    /* Policy: binary frames are accepted and treated as STOMP bytes. */
    unsigned char frame[64];
    size_t n = make_frame(frame, 1, WS_OP_BINARY, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, frame, n, &r) == WS_COMPLETE_DATAFRAME);
    WS_CHECK(messages_equal(&r, "SEND\n\n", 7) && !r.close);
    ws_input_result_free(&r);
}

static void test_ping_does_not_wedge_connection(void) {
    unsigned char buf[128];
    size_t n = make_frame(buf, 1, 9, "hello", 5, 1);
    n += make_frame(buf + n, 1, 1, "SEND\ndestination:/a\n\n", 22, 1);

    int fd = new_test_fd();
    ws_input_result r;
    feed(fd, buf, n, &r);
    WS_CHECK(messages_equal(&r, "SEND\ndestination:/a\n\n", 22));
    WS_CHECK(pending_bytes(fd) == 0 && !r.close);
    ws_input_result_free(&r);
}

/** Expects a broker initiated close with the given code and nothing left buffered. */
static void check_rejected(const unsigned char *buf, size_t n, uint16_t code) {
    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, buf, n, &r) < 0);
    WS_CHECK(r.close == 1 && r.close_code == code);
    WS_CHECK(r.replies_len == 4 && (unsigned char) r.replies[0] == 0x88);
    WS_CHECK(r.replies_len == 4 && (((unsigned char) r.replies[2] << 8) | (unsigned char) r.replies[3]) == code);
    WS_CHECK(r.messages == NULL);
    WS_CHECK(ws_buffer_find(fd) == NULL);
    ws_input_result_free(&r);
}

static void test_unknown_opcode_is_consumed(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, 3, "abcdefghijklmnop", 16, 1);
    check_rejected(buf, n, WS_CLOSE_PROTOCOL_ERROR);
}

static void test_reject_unmasked_frame(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 0);
    check_rejected(buf, n, WS_CLOSE_PROTOCOL_ERROR);
}

static void test_reject_reserved_bits(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    buf[0] |= 0x40; /* RSV1 without a negotiated extension */
    check_rejected(buf, n, WS_CLOSE_PROTOCOL_ERROR);
}

static void test_reject_bad_control_frames(void) {
    unsigned char buf[256];
    char *big = repeat_char('p', 126);
    size_t n = make_frame(buf, 1, WS_OP_PING, big, 126, 1); /* control payload > 125 */
    check_rejected(buf, n, WS_CLOSE_PROTOCOL_ERROR);
    n = make_frame(buf, 0, WS_OP_PING, "x", 1, 1); /* fragmented control frame */
    check_rejected(buf, n, WS_CLOSE_PROTOCOL_ERROR);
    n = make_frame(buf, 1, WS_OP_CLOSE, "x", 1, 1); /* 1 byte close payload */
    check_rejected(buf, n, WS_CLOSE_PROTOCOL_ERROR);
    free(big);
}

static void test_reject_too_large_frame(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(ws_input_filter_dataframe(fd, (char *) buf, n, 4, &r) == WS_TOO_LARGE_DATAFRAME);
    WS_CHECK(r.close && r.close_code == WS_CLOSE_MESSAGE_TOO_BIG && ws_buffer_find(fd) == NULL);
    ws_input_result_free(&r);
}

/**
 * Fuzz: random bytes (fixed seed). The filter must always either consume
 * everything it can, keep only a partial frame, or close - never leave a
 * decodable/invalid frame stuck at the buffer head.
 */
static void test_fuzz_random_bytes_always_progress(void) {
    srand(6455);
    for (int round = 0; round < 3000; round++) {
        int fd = new_test_fd();
        unsigned char buf[64];
        size_t total = 1 + (size_t) (rand() % (int) sizeof (buf));
        for (size_t i = 0; i < total; i++) buf[i] = (unsigned char) rand();
        if (round % 2) buf[1] |= 0x80; /* mostly-valid masked headers half the time */

        size_t fed = 0;
        int closed = 0;
        while (fed < total && !closed) {
            size_t chunk = 1 + (size_t) (rand() % 8);
            if (chunk > total - fed) chunk = total - fed;
            ws_input_result r;
            ws_input_filter_dataframe(fd, (char *) buf + fed, chunk, 1024, &r);
            fed += chunk;
            closed = r.close;
            ws_input_result_free(&r);

            /* whatever is still pending must be an incomplete frame */
            buffer_item *slot = ws_buffer_find(fd);
            if (!closed && slot && slot->received_len > 0) {
                ws_frame f;
                WS_CHECK(ws_decode_frame((unsigned char *) slot->received, slot->received_len, 1024, &f)
                        == WS_DECODE_NEED_MORE);
            }
        }
        if (closed) WS_CHECK(ws_buffer_find(fd) == NULL);
        ws_buffer_free(ws_buffer_find(fd));
        session_storage_remove(fd);
    }
}

/* ---------------------------------------------------------------------------
 * Control frames (gap 3)
 * ------------------------------------------------------------------------- */
static void test_ping_gets_pong(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, WS_OP_PING, "beat", 4, 1);
    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, buf, n, &r) == WS_INCOMPLETE_DATAFRAME);
    const unsigned char pong[] = {0x8A, 0x04, 'b', 'e', 'a', 't'};
    WS_CHECK(r.replies_len == sizeof (pong) && memcmp(r.replies, pong, sizeof (pong)) == 0);
    WS_CHECK(!r.close && r.messages == NULL);
    ws_input_result_free(&r);
}

static void test_empty_ping_gets_empty_pong(void) {
    unsigned char buf[16];
    size_t n = make_frame(buf, 1, WS_OP_PING, "", 0, 1);
    int fd = new_test_fd();
    ws_input_result r;
    feed(fd, buf, n, &r);
    WS_CHECK(r.replies_len == 2 && (unsigned char) r.replies[0] == 0x8A && r.replies[1] == 0);
    ws_input_result_free(&r);
}

static void test_pong_is_ignored(void) {
    unsigned char buf[16];
    size_t n = make_frame(buf, 1, WS_OP_PONG, "x", 1, 1);
    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, buf, n, &r) == WS_INCOMPLETE_DATAFRAME);
    WS_CHECK(r.replies_len == 0 && !r.close && pending_bytes(fd) == 0);
    ws_input_result_free(&r);
}

static void test_close_is_echoed(void) {
    unsigned char buf[64];
    const char payload[] = {0x03, (char) 0xE8, 'b', 'y', 'e'}; /* 1000 + reason */
    size_t n = make_frame(buf, 1, WS_OP_CLOSE, payload, sizeof (payload), 1);
    n += make_frame(buf + n, 1, WS_OP_TEXT, SMALL_STOMP, SMALL_STOMP_LEN, 1); /* after close: ignored */
    int fd = new_test_fd();
    ws_input_result r;
    WS_CHECK(feed(fd, buf, n, &r) == WS_OPCODE_CLIENT_DISCONNECT);
    const unsigned char echo[] = {0x88, 0x02, 0x03, 0xE8};
    WS_CHECK(r.replies_len == sizeof (echo) && memcmp(r.replies, echo, sizeof (echo)) == 0);
    WS_CHECK(r.close == 1 && r.close_code == 1000 && r.messages == NULL);
    WS_CHECK(ws_buffer_find(fd) == NULL);
    ws_input_result_free(&r);
}

static void test_close_without_code_is_echoed_empty(void) {
    unsigned char buf[16];
    size_t n = make_frame(buf, 1, WS_OP_CLOSE, "", 0, 1);
    int fd = new_test_fd();
    ws_input_result r;
    feed(fd, buf, n, &r);
    WS_CHECK(r.replies_len == 2 && (unsigned char) r.replies[0] == 0x88 && r.replies[1] == 0);
    WS_CHECK(r.close == 1 && r.close_code == WS_CLOSE_NORMAL);
    ws_input_result_free(&r);
}

static void test_messages_before_close_are_delivered(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, WS_OP_TEXT, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    n += make_frame(buf + n, 1, WS_OP_CLOSE, "\x03\xe8", 2, 1);
    int fd = new_test_fd();
    ws_input_result r;
    feed(fd, buf, n, &r);
    WS_CHECK(messages_equal(&r, "SEND\n\n", 7) && r.close == 1);
    ws_input_result_free(&r);
}

/* ---------------------------------------------------------------------------
 * Fragmentation (gap 4)
 * ------------------------------------------------------------------------- */
static void test_fragments_with_ping_between(void) {
    unsigned char buf[256];
    size_t n = make_frame(buf, 0, WS_OP_TEXT, "SEND\ndesti", 10, 1);
    n += make_frame(buf + n, 1, WS_OP_PING, "p1", 2, 1);
    n += make_frame(buf + n, 0, WS_OP_CONTINUATION, "nation:/a\n", 10, 1);
    n += make_frame(buf + n, 1, WS_OP_PONG, "", 0, 1);
    n += make_frame(buf + n, 1, WS_OP_CONTINUATION, "\nbody", 6, 1);
    n += make_frame(buf + n, 1, WS_OP_TEXT, SMALL_STOMP, SMALL_STOMP_LEN, 1);

    /* deliver in every chunk size from 1 byte to all at once */
    for (size_t chunk = 1; chunk <= n; chunk++) {
        int fd = new_test_fd();
        char *messages = NULL, *replies = NULL;
        size_t messages_len = 0, replies_len = 0;
        for (size_t off = 0; off < n; off += chunk) {
            size_t len = off + chunk > n ? n - off : chunk;
            ws_input_result r;
            feed(fd, buf + off, len, &r);
            WS_CHECK(!r.close);
            append_bytes(&messages, &messages_len, r.messages, r.messages_len);
            append_bytes(&replies, &replies_len, r.replies, r.replies_len);
            ws_input_result_free(&r);
        }
        WS_CHECK(messages_len == 33 && memcmp(messages, "SEND\ndestination:/a\n\nbody\0SEND\n\n\0", 33) == 0);
        const unsigned char pong[] = {0x8A, 0x02, 'p', '1'};
        WS_CHECK(replies_len == sizeof (pong) && memcmp(replies, pong, sizeof (pong)) == 0);
        WS_CHECK(ws_buffer_find(fd) == NULL);
        free(messages);
        free(replies);
    }
}

static void test_fragmented_binary_message(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 0, WS_OP_BINARY, "SEN", 3, 1);
    n += make_frame(buf + n, 1, WS_OP_CONTINUATION, "D\n\n", 4, 1);
    int fd = new_test_fd();
    ws_input_result r;
    feed(fd, buf, n, &r);
    WS_CHECK(messages_equal(&r, "SEND\n\n", 7) && !r.close);
    ws_input_result_free(&r);
}

static void test_fragmented_message_too_big(void) {
    unsigned char buf[128];
    size_t n = make_frame(buf, 0, WS_OP_TEXT, "0123456789", 10, 1);
    n += make_frame(buf + n, 0, WS_OP_CONTINUATION, "0123456789", 10, 1);
    n += make_frame(buf + n, 1, WS_OP_CONTINUATION, "0123456789", 10, 1);
    int fd = new_test_fd();
    ws_input_result r;
    /* each frame fits (10 <= 25) but the message does not (30 > 25) */
    WS_CHECK(ws_input_filter_dataframe(fd, (char *) buf, n, 25, &r) == WS_TOO_LARGE_DATAFRAME);
    WS_CHECK(r.close && r.close_code == WS_CLOSE_MESSAGE_TOO_BIG && r.messages == NULL);
    WS_CHECK(r.replies_len == 4 && (unsigned char) r.replies[2] == 0x03 && (unsigned char) r.replies[3] == 0xF1);
    WS_CHECK(ws_buffer_find(fd) == NULL);
    ws_input_result_free(&r);
}

static void test_continuation_without_start_rejected(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, WS_OP_CONTINUATION, "abc", 3, 1);
    check_rejected(buf, n, WS_CLOSE_PROTOCOL_ERROR);
}

static void test_new_message_inside_fragmented_rejected(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 0, WS_OP_TEXT, "abc", 3, 1);
    n += make_frame(buf + n, 1, WS_OP_TEXT, "def", 3, 1);
    check_rejected(buf, n, WS_CLOSE_PROTOCOL_ERROR);
}

/* ---------------------------------------------------------------------------
 * Output messages
 * ------------------------------------------------------------------------- */
static void test_raw_output_message_is_written_verbatim(void) {
    const char bytes[] = {(char) 0x88, 0x02, 0x03, (char) 0xEA};
    message_with_frame_len *m = message_create_raw(42, bytes, sizeof (bytes), 1);
    WS_CHECK(m->raw == 1 && m->close_after == 1 && m->frame_len == 4);
    WS_CHECK(ws_output_filter(m) == 4 && memcmp(m->content, bytes, 4) == 0 && m->fd == 42);
    message_destroy_with_frame_len(m);
}

static void test_frame_len_matches_encoded_size(void) {
    size_t sizes[] = {0, 124, 125, 126, 65534, 65535, 70000};
    int fd = new_test_fd();
    for (size_t i = 0; i < sizeof (sizes) / sizeof (sizes[0]); i++) {
        char *body = repeat_char('f', sizes[i]);
        message_with_frame_len *m = message_create_with_frame_len(session_set_encoded(fd), body);
        size_t predicted = m->frame_len;
        WS_CHECK(ws_output_filter(m) == predicted);
        message_destroy_with_frame_len(m);
        m = message_create_with_frame_len(fd, body); /* raw STOMP: body + NUL */
        WS_CHECK(m->frame_len == sizes[i] + 1 && ws_output_filter(m) == sizes[i] + 1);
        message_destroy_with_frame_len(m);
        free(body);
    }
}

static void test_fragmented_message(void) {
    unsigned char buf[128];
    size_t n = make_frame(buf, 0, 1, "SEND\ndesti", 10, 1);
    n += make_frame(buf + n, 0, 0, "nation:/a\n", 10, 1);
    n += make_frame(buf + n, 1, 0, "\nbody", 6, 1); /* incl. NUL */

    int fd = new_test_fd();
    ws_input_result r;
    feed(fd, buf, n, &r);
    WS_CHECK(messages_equal(&r, "SEND\ndestination:/a\n\nbody", 26));
    WS_CHECK(!r.close);
    ws_input_result_free(&r);
}

/* ---------------------------------------------------------------------------
 * Pure frame codec
 * ------------------------------------------------------------------------- */
static void check_decode_roundtrip(size_t len) {
    char *body = repeat_char('p', len);
    unsigned char *frame = malloc(len + 16);
    size_t n = make_frame(frame, 1, WS_OP_TEXT, body, len, 1);
    ws_frame f;
    WS_CHECK(ws_decode_frame(frame, n, WS_DATA_FRAME_MAX_LENGTH, &f) == WS_DECODE_OK);
    WS_CHECK(f.consumed == n && f.payload_len == len && f.fin == 1 && f.opcode == WS_OP_TEXT);
    WS_CHECK(f.payload != NULL && memcmp(f.payload, body, len) == 0 && f.payload[len] == '\0');
    free(f.payload);
    /* every strict prefix is incomplete and consumes nothing */
    for (size_t k = 0; k < n; k += (n > 300 ? 97 : 1)) {
        WS_CHECK(ws_decode_frame(frame, k, WS_DATA_FRAME_MAX_LENGTH, &f) == WS_DECODE_NEED_MORE);
        WS_CHECK(f.consumed == 0 && f.payload == NULL);
    }
    free(frame);
    free(body);
}

static void test_codec_decode_all_length_encodings(void) {
    size_t sizes[] = {0, 1, 7, 125, 126, 127, 65535, 65536, 70000};
    for (size_t i = 0; i < sizeof (sizes) / sizeof (sizes[0]); i++) check_decode_roundtrip(sizes[i]);
}

static void test_codec_masking(void) {
    /* RFC 6455 5.7: masked "Hello" */
    const unsigned char hello[] = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58};
    ws_frame f;
    WS_CHECK(ws_decode_frame(hello, sizeof (hello), 1000, &f) == WS_DECODE_OK);
    WS_CHECK(f.payload_len == 5 && memcmp(f.payload, "Hello", 5) == 0 && f.consumed == sizeof (hello));
    free(f.payload);
}

static void test_codec_frame_too_large(void) {
    unsigned char frame[256];
    char *body = repeat_char('b', 200);
    size_t n = make_frame(frame, 1, WS_OP_TEXT, body, 200, 1);
    ws_frame f;
    WS_CHECK(ws_decode_frame(frame, n, 100, &f) == WS_DECODE_ERROR && f.close_code == WS_CLOSE_MESSAGE_TOO_BIG);
    /* detected from the header alone, without waiting for the payload */
    WS_CHECK(ws_decode_frame(frame, 4, 100, &f) == WS_DECODE_ERROR && f.close_code == WS_CLOSE_MESSAGE_TOO_BIG);
    free(body);
}

static void check_encode(size_t len, size_t header_len, unsigned char len_byte) {
    char *body = repeat_char('e', len);
    char *out = NULL;
    size_t n = ws_encode_frame(WS_OP_TEXT, body, len, &out);
    WS_CHECK(n == header_len + len);
    WS_CHECK((unsigned char) out[0] == 0x81 && (unsigned char) out[1] == len_byte);
    if (header_len == 4) WS_CHECK((((unsigned char) out[2] << 8) | (unsigned char) out[3]) == len);
    if (header_len == 10) {
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) v = (v << 8) | (unsigned char) out[2 + i];
        WS_CHECK(v == len);
    }
    WS_CHECK(memcmp(out + header_len, body, len) == 0);
    WS_CHECK(ws_frame_header_len(len) == header_len);
    free(out);
    free(body);
}

static void test_codec_encode_length_boundaries(void) {
    check_encode(0, 2, 0);
    check_encode(125, 2, 125);
    check_encode(126, 4, 126);
    check_encode(65535, 4, 126);
    check_encode(65536, 10, 127);
}

static void test_codec_encode_control(void) {
    char *out = NULL;
    size_t n = ws_encode_close(WS_CLOSE_PROTOCOL_ERROR, &out);
    WS_CHECK(n == 4 && (unsigned char) out[0] == 0x88 && out[1] == 2);
    WS_CHECK((unsigned char) out[2] == 0x03 && (unsigned char) out[3] == 0xEA); /* 1002 */
    free(out);
    char *big = repeat_char('c', 200);
    n = ws_encode_control(WS_OP_PONG, big, 200, &out);
    WS_CHECK(n == 2 + 125 && (unsigned char) out[0] == 0x8A && out[1] == 125);
    free(out);
    free(big);
}

/* ---------------------------------------------------------------------------
 * Handshake tests (gap 5), written against the current handshake contract.
 * ------------------------------------------------------------------------- */
#define RFC_SAMPLE_KEY "dGhlIHNhbXBsZSBub25jZQ=="
#define RFC_SAMPLE_ACCEPT "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="

static void test_accept_key_rfc_sample(void) {
    char *accept = create_accept_key(RFC_SAMPLE_KEY);
    WS_CHECK(accept != NULL && strcmp(accept, RFC_SAMPLE_ACCEPT) == 0);
    free(accept);
}

#define REQ_LINE "GET /chat HTTP/1.1\r\n"
#define REQ_STD_HEADERS "Host: server.example.com\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
#define REQ_KEY "Sec-WebSocket-Key: " RFC_SAMPLE_KEY "\r\n"
#define REQ_V13 "Sec-WebSocket-Version: 13\r\n"
#define VALID_REQUEST REQ_LINE REQ_STD_HEADERS REQ_KEY REQ_V13 "\r\n"

/** Runs the pure handshake; returns the status, response copied to *resp (caller frees). */
static int handshake(const char *request, const ws_handshake_policy *policy, char **resp, char *protocol) {
    ws_handshake_result r;
    int status = ws_handshake_process(request, strlen(request), policy, &r);
    *resp = r.response ? strndup(r.response, r.response_len) : NULL;
    if (protocol) strcpy(protocol, r.protocol);
    ws_handshake_result_free(&r);
    return status;
}

static int starts_with(const char *s, const char *prefix) {
    return s != NULL && strncmp(s, prefix, strlen(prefix)) == 0;
}

static void test_handshake_valid(void) {
    char *resp;
    WS_CHECK(handshake(VALID_REQUEST, NULL, &resp, NULL) == WS_HANDSHAKE_OK);
    WS_CHECK(starts_with(resp, "HTTP/1.1 101 Switching Protocols\r\n"));
    WS_CHECK(resp != NULL && strstr(resp, "\r\nSec-WebSocket-Accept: " RFC_SAMPLE_ACCEPT "\r\n") != NULL);
    WS_CHECK(resp != NULL && strstr(resp, "\r\nUpgrade: websocket\r\n") != NULL);
    WS_CHECK(resp != NULL && strstr(resp, "Sec-WebSocket-Protocol") == NULL);
    WS_CHECK(resp != NULL && strlen(resp) > 4 && strcmp(resp + strlen(resp) - 4, "\r\n\r\n") == 0);
    free(resp);
}

static void test_handshake_mixed_case_headers(void) {
    char *resp;
    WS_CHECK(handshake(REQ_LINE "host: server.example.com\r\nupgrade: WebSocket\r\n"
            "connection: keep-alive, Upgrade\r\nsec-websocket-key:   " RFC_SAMPLE_KEY "  \r\n"
            "SEC-WEBSOCKET-VERSION: 13\r\n\r\n", NULL, &resp, NULL) == WS_HANDSHAKE_OK);
    WS_CHECK(resp != NULL && strstr(resp, RFC_SAMPLE_ACCEPT) != NULL);
    free(resp);
}

static void test_handshake_wrong_version(void) {
    char *resp;
    WS_CHECK(handshake(REQ_LINE REQ_STD_HEADERS REQ_KEY "Sec-WebSocket-Version: 8\r\n\r\n", NULL, &resp, NULL)
            == WS_HANDSHAKE_UPGRADE_REQUIRED);
    WS_CHECK(starts_with(resp, "HTTP/1.1 426"));
    WS_CHECK(resp != NULL && strstr(resp, "\r\nSec-WebSocket-Version: 13\r\n") != NULL);
    free(resp);
    WS_CHECK(handshake(REQ_LINE REQ_STD_HEADERS REQ_KEY "\r\n", NULL, &resp, NULL) == WS_HANDSHAKE_UPGRADE_REQUIRED);
    free(resp);
}

static void test_handshake_missing_key(void) {
    char *resp;
    WS_CHECK(handshake(REQ_LINE REQ_STD_HEADERS REQ_V13 "\r\n", NULL, &resp, NULL) == WS_HANDSHAKE_BAD_REQUEST);
    WS_CHECK(starts_with(resp, "HTTP/1.1 400"));
    free(resp);
}

static void test_handshake_bad_requests(void) {
    const char *bad[] = {
        "POST /chat HTTP/1.1\r\n" REQ_STD_HEADERS REQ_KEY REQ_V13 "\r\n", /* method */
        "GET /chat HTTP/1.0\r\n" REQ_STD_HEADERS REQ_KEY REQ_V13 "\r\n", /* version */
        "GET  HTTP/1.1\r\n" REQ_STD_HEADERS REQ_KEY REQ_V13 "\r\n", /* empty target */
        REQ_LINE "Upgrade: websocket\r\nConnection: Upgrade\r\n" REQ_KEY REQ_V13 "\r\n", /* no Host */
        REQ_LINE "Host: h\r\nConnection: Upgrade\r\n" REQ_KEY REQ_V13 "\r\n", /* no Upgrade */
        REQ_LINE "Host: h\r\nUpgrade: websocket\r\nConnection: keep-alive\r\n" REQ_KEY REQ_V13 "\r\n",
        REQ_LINE REQ_STD_HEADERS "Sec-WebSocket-Key: c2hvcnQ=\r\n" REQ_V13 "\r\n", /* 4 byte key */
        REQ_LINE REQ_STD_HEADERS "Sec-WebSocket-Key: !!!!!!!!!!!!!!!!!!!!!!==\r\n" REQ_V13 "\r\n",
        REQ_LINE REQ_STD_HEADERS "this line has no colon\r\n" REQ_KEY REQ_V13 "\r\n",
    };
    for (size_t i = 0; i < sizeof (bad) / sizeof (bad[0]); i++) {
        char *resp;
        int status = handshake(bad[i], NULL, &resp, NULL);
        WS_CHECK(status == WS_HANDSHAKE_BAD_REQUEST);
        if (status != WS_HANDSHAKE_BAD_REQUEST) printf("      case %zu\n", i);
        WS_CHECK(starts_with(resp, "HTTP/1.1 400"));
        free(resp);
    }
}

static void test_handshake_oversize_request(void) {
    size_t len = WS_HANDSHAKE_MAX_SIZE + 10;
    char *req = repeat_char('a', len);
    memcpy(req, "GET / HTTP/1.1\r\nX-Pad: ", 23);
    ws_handshake_result r;
    WS_CHECK(ws_handshake_process(req, len, NULL, &r) == WS_HANDSHAKE_BAD_REQUEST);
    ws_handshake_result_free(&r);
    free(req);
}

static void test_handshake_subprotocol(void) {
    char *resp;
    char protocol[16];
    WS_CHECK(handshake(REQ_LINE REQ_STD_HEADERS REQ_KEY REQ_V13
            "Sec-WebSocket-Protocol: v10.stomp, v11.stomp, v12.stomp\r\n\r\n", NULL, &resp, protocol) == WS_HANDSHAKE_OK);
    WS_CHECK(strcmp(protocol, "v12.stomp") == 0);
    WS_CHECK(resp != NULL && strstr(resp, "\r\nSec-WebSocket-Protocol: v12.stomp\r\n") != NULL);
    free(resp);

    WS_CHECK(handshake(REQ_LINE REQ_STD_HEADERS REQ_KEY REQ_V13
            "Sec-WebSocket-Protocol: mqtt\r\nsec-websocket-protocol: V10.STOMP\r\n\r\n", NULL, &resp, protocol) == WS_HANDSHAKE_OK);
    WS_CHECK(strcmp(protocol, "v10.stomp") == 0);
    free(resp);

    WS_CHECK(handshake(REQ_LINE REQ_STD_HEADERS REQ_KEY REQ_V13
            "Sec-WebSocket-Protocol: mqtt, wamp\r\n\r\n", NULL, &resp, protocol) == WS_HANDSHAKE_OK);
    WS_CHECK(protocol[0] == '\0' && resp != NULL && strstr(resp, "Sec-WebSocket-Protocol") == NULL);
    free(resp);
}

static void test_handshake_origin_and_host_policy(void) {
    char *resp;
    const char *from_good = REQ_LINE REQ_STD_HEADERS REQ_KEY REQ_V13 "Origin: https://good.example\r\n\r\n";
    const char *from_evil = REQ_LINE REQ_STD_HEADERS REQ_KEY REQ_V13 "Origin: https://evil.example\r\n\r\n";

    /* default: allow everything */
    WS_CHECK(handshake(from_evil, NULL, &resp, NULL) == WS_HANDSHAKE_OK);
    free(resp);
    ws_handshake_policy empty = {"", ""};
    WS_CHECK(handshake(from_evil, &empty, &resp, NULL) == WS_HANDSHAKE_OK);
    free(resp);

    ws_handshake_policy origins = {"https://a.example, HTTPS://GOOD.EXAMPLE", NULL};
    WS_CHECK(handshake(from_good, &origins, &resp, NULL) == WS_HANDSHAKE_OK);
    free(resp);
    WS_CHECK(handshake(from_evil, &origins, &resp, NULL) == WS_HANDSHAKE_FORBIDDEN);
    WS_CHECK(starts_with(resp, "HTTP/1.1 403"));
    free(resp);
    WS_CHECK(handshake(VALID_REQUEST, &origins, &resp, NULL) == WS_HANDSHAKE_OK); /* no Origin: non-browser */
    free(resp);

    ws_handshake_policy hosts = {NULL, "server.example.com"};
    WS_CHECK(handshake(VALID_REQUEST, &hosts, &resp, NULL) == WS_HANDSHAKE_OK);
    free(resp);
    WS_CHECK(handshake(REQ_LINE "Host: server.example.com:3490\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            REQ_KEY REQ_V13 "\r\n", &hosts, &resp, NULL) == WS_HANDSHAKE_OK);
    free(resp);
    WS_CHECK(handshake(REQ_LINE "Host: other.example\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            REQ_KEY REQ_V13 "\r\n", &hosts, &resp, NULL) == WS_HANDSHAKE_FORBIDDEN);
    free(resp);
}

static void test_handshake_request_start_detection(void) {
    WS_CHECK(ws_is_http_request_start("GET / HTTP/1.1", 14) == 1);
    WS_CHECK(ws_is_http_request_start("GE", 2) == 0);
    WS_CHECK(ws_is_http_request_start("CONNECT\n", 8) == -1);
    WS_CHECK(ws_is_http_request_start("SEND\n", 5) == -1);
    WS_CHECK(ws_is_http_request_start("GETX", 4) == -1);
}

static void test_handshake_split_request(void) {
    const char *req = VALID_REQUEST "\x81\x80\x01\x02\x03\x04"; /* + an early empty frame */
    size_t len = strlen(VALID_REQUEST) + 6;
    for (size_t k = 1; k < strlen(VALID_REQUEST); k++) {
        int fd = new_test_fd();
        ws_handshake_result r;
        WS_CHECK(ws_input_filter_handshake(fd, req, k, NULL, &r) == WS_HANDSHAKE_INCOMPLETE);
        WS_CHECK(r.response == NULL && pending_bytes(fd) == k);
        ws_handshake_result_free(&r);
        WS_CHECK(ws_input_filter_handshake(fd, req + k, len - k, NULL, &r) == WS_HANDSHAKE_OK);
        WS_CHECK(r.response != NULL && strstr(r.response, RFC_SAMPLE_ACCEPT) != NULL);
        WS_CHECK(pending_bytes(fd) == 6); /* the early frame stays as WebSocket input */
        ws_handshake_result_free(&r);
        ws_buffer_free(ws_buffer_find(fd));
    }
}

static void test_handshake_failure_clears_buffer(void) {
    int fd = new_test_fd();
    const char *req = REQ_LINE REQ_STD_HEADERS REQ_V13 "\r\n";
    ws_handshake_result r;
    WS_CHECK(ws_input_filter_handshake(fd, req, 10, NULL, &r) == WS_HANDSHAKE_INCOMPLETE);
    ws_handshake_result_free(&r);
    WS_CHECK(ws_input_filter_handshake(fd, req + 10, strlen(req) - 10, NULL, &r) == WS_HANDSHAKE_BAD_REQUEST);
    WS_CHECK(ws_buffer_find(fd) == NULL);
    ws_handshake_result_free(&r);
}

/* ---------------------------------------------------------------------------
 * Per-connection transport state (gap 6)
 * ------------------------------------------------------------------------- */
static void test_route_raw_stomp_stays_raw(void) {
    int fd = new_test_fd();
    ws_conn_reset(fd);
    WS_CHECK(ws_conn_route(fd, "CONNECT\naccept-version:1.2\n\n", 29) == CONN_RAW_STOMP);
    /* a later read that happens to start with "GET /" is still STOMP payload */
    WS_CHECK(ws_conn_route(fd, "GET / HTTP/1.1\r\n", 16) == CONN_RAW_STOMP);
    WS_CHECK(ws_conn_route(fd, "\n", 1) == CONN_RAW_STOMP);
    ws_conn_reset(fd);
}

static void test_route_upgrade_then_frames(void) {
    int fd = new_test_fd();
    ws_conn_reset(fd);
    WS_CHECK(ws_conn_route(fd, "GE", 2) == CONN_HTTP_PENDING);
    WS_CHECK(ws_conn_route(fd, "T / HTTP/1.1", 12) == CONN_HTTP_PENDING);
    ws_conn_set_state(fd, CONN_WS_OPEN); /* what the server does after a 101 */
    WS_CHECK(ws_conn_route(fd, "GET / HTTP/1.1\r\n", 16) == CONN_WS_OPEN);

    /* a STOMP frame whose body contains "GET /" is just a message */
    unsigned char buf[128];
    const char stomp[] = "SEND\ndestination:/a\n\nGET / HTTP/1.1\r\n";
    size_t n = make_frame(buf, 1, WS_OP_TEXT, stomp, sizeof (stomp), 1);
    ws_input_result r;
    WS_CHECK(feed(fd, buf, n, &r) == WS_COMPLETE_DATAFRAME);
    WS_CHECK(messages_equal(&r, stomp, sizeof (stomp)) && r.replies_len == 0);
    ws_input_result_free(&r);

    ws_conn_reset(fd);
    WS_CHECK(ws_conn_get_state(fd) == CONN_NEW);
}

static void test_route_empty_read_keeps_new(void) {
    int fd = new_test_fd();
    ws_conn_reset(fd);
    WS_CHECK(ws_conn_route(fd, "", 0) == CONN_NEW);
}

/* ---------------------------------------------------------------------------
 * Output path (gap 9): bounded batching + partial writes to a slow reader
 * ------------------------------------------------------------------------- */
typedef struct slow_reader_t {
    int fd;
    size_t expected;
    char *received;
    size_t received_len;
} slow_reader;

static void *slow_reader_thread(void *arg) {
    slow_reader *r = arg;
    char chunk[700];
    while (r->received_len < r->expected) {
        usleep(300); /* slower than the writer */
        ssize_t n = read(r->fd, chunk, sizeof (chunk));
        if (n <= 0) break;
        memcpy(r->received + r->received_len, chunk, (size_t) n);
        r->received_len += (size_t) n;
    }
    return NULL;
}

static int nonblocking_pair(int sv[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return -1;
    int small = 2048;
    setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &small, sizeof (small));
    setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &small, sizeof (small));
    fcntl(sv[0], F_SETFL, fcntl(sv[0], F_GETFL) | O_NONBLOCK);
    return 0;
}

static void test_output_larger_than_buffer_to_slow_reader(void) {
    int sv[2];
    WS_CHECK(nonblocking_pair(sv) == 0);
    int fd = new_test_fd();

    /* messages much larger than the 10000 byte writer buffer, mixed with small ones */
    size_t sizes[] = {50000, 100, 9990, 30000, 1, 70000};
    size_t count = sizeof (sizes) / sizeof (sizes[0]);
    char *expected = NULL;
    size_t expected_len = 0;
    char write_buffer[10000];
    output_batch batch = {.fd = sv[0], .buffer = write_buffer, .capacity = sizeof (write_buffer), .used = 0, .timeout_ms = 5000};

    slow_reader reader = {.fd = sv[1], .expected = 0, .received = NULL, .received_len = 0};
    /* precompute the expected stream so the reader knows when to stop */
    for (size_t i = 0; i < count; i++) {
        char *body = repeat_char((char) ('a' + i), sizes[i]);
        message_with_frame_len *m = message_create_with_frame_len(session_set_encoded(fd), body);
        size_t len = ws_output_filter(m);
        append_bytes(&expected, &expected_len, m->content, len);
        message_destroy_with_frame_len(m);
        free(body);
    }
    reader.expected = expected_len;
    reader.received = malloc(expected_len);
    pthread_t tid;
    pthread_create(&tid, NULL, slow_reader_thread, &reader);

    int failed = 0;
    for (size_t i = 0; i < count; i++) { /* same flow as writer_thread */
        char *body = repeat_char((char) ('a' + i), sizes[i]);
        message_with_frame_len *m = message_create_with_frame_len(session_set_encoded(fd), body);
        size_t len = ws_output_filter(m);
        failed |= output_batch_add(&batch, m->content, len) < 0;
        WS_CHECK(batch.used <= batch.capacity);
        message_destroy_with_frame_len(m);
        free(body);
    }
    failed |= output_batch_flush(&batch) < 0;
    pthread_join(tid, NULL);

    WS_CHECK(!failed);
    WS_CHECK(reader.received_len == expected_len);
    WS_CHECK(reader.received_len == expected_len && memcmp(reader.received, expected, expected_len) == 0);
    free(reader.received);
    free(expected);
    close(sv[0]);
    close(sv[1]);
}

static void test_output_gives_up_after_timeout(void) {
    int sv[2];
    WS_CHECK(nonblocking_pair(sv) == 0);
    size_t len = 1000000;
    char *bytes = repeat_char('t', len);
    long long start = monotonic_ms();
    WS_CHECK(write_fully(sv[0], bytes, len, 150) == -1); /* nobody reads */
    long long elapsed = monotonic_ms() - start;
    WS_CHECK(elapsed >= 140 && elapsed < 3000);
    free(bytes);
    close(sv[0]);
    close(sv[1]);
}

typedef struct trickle_reader_t {
    int fd;
    size_t chunk;
    useconds_t pause_us;
} trickle_reader;

/* keeps reading a little at a time until the writer end is closed */
static void *trickle_reader_thread(void *arg) {
    trickle_reader *r = arg;
    char buf[4096];
    while (read(r->fd, buf, r->chunk) > 0) usleep(r->pause_us);
    return NULL;
}

/*
 * Sends one small batch per round (as writer_thread does) to a reader that
 * takes ~100 KB/s, with a 150 ms wait budget. Every single batch completes
 * well inside 150 ms, so only the per-connection budget can catch it.
 * @return elapsed ms until the connection was dropped, or -1 if it never was
 */
static long long batches_to_trickling_reader(size_t min_bytes, long long run_ms) {
    int sv[2];
    WS_CHECK(nonblocking_pair(sv) == 0);
    trickle_reader reader = {.fd = sv[1], .chunk = 1024, .pause_us = 10000};
    pthread_t tid;
    pthread_create(&tid, NULL, trickle_reader_thread, &reader);

    char write_buffer[256], payload[200];
    memset(payload, 'p', sizeof (payload));
    output_stall stall = {0};
    int failed = 0;
    long long start = monotonic_ms();
    while (!failed && monotonic_ms() - start < run_ms) {
        output_batch batch = {.fd = sv[0], .buffer = write_buffer, .capacity = sizeof (write_buffer), .used = 0,
            .timeout_ms = 150, .min_bytes = min_bytes, .stall = &stall};
        failed = output_batch_add(&batch, payload, sizeof (payload)) < 0 || output_batch_flush(&batch) < 0;
    }
    long long elapsed = monotonic_ms() - start;

    close(sv[0]);
    pthread_join(tid, NULL);
    close(sv[1]);
    return failed ? elapsed : -1;
}

static void test_output_drops_trickling_reader(void) {
    /* required: 1 MB per 150 ms of waiting; the reader is far slower */
    long long dropped_after = batches_to_trickling_reader(1 << 20, 3000);
    WS_CHECK(dropped_after >= 0);
    WS_CHECK(dropped_after < 1500);
}

static void test_output_keeps_reader_above_min_rate(void) {
    /* required: 1 KB per 150 ms of waiting; the reader is much faster */
    WS_CHECK(batches_to_trickling_reader(1024, 1000) == -1);
}

static void test_output_peer_closed_fails(void) {
    int sv[2];
    WS_CHECK(nonblocking_pair(sv) == 0);
    close(sv[1]);
    WS_CHECK(write_fully(sv[0], "abc", 3, 1000) == -1);
    close(sv[0]);
}

/* ---------------------------------------------------------------------------
 * Output queue: per-connection ordering across raw and encoded messages
 * ------------------------------------------------------------------------- */
typedef message_with_frame_len *output_batch_messages[MESSAGES_BATCH_SIZE];

static int take_batch(ts_queue *q, output_batch_messages *batch) {
    for (int i = 0; i < MESSAGES_BATCH_SIZE; i++) (*batch)[i] = NULL;
    return output_queue_take_batch(batch, q, 10000);
}

/* checks the batch holds exactly `expected` (in order), then frees it */
static void check_batch_contents(output_batch_messages *batch, const char *const *expected, int count) {
    int i = 0;
    for (; i < MESSAGES_BATCH_SIZE && (*batch)[i] != NULL; i++) {
        WS_CHECK(i < count && strcmp((*batch)[i]->content, expected[i]) == 0);
        message_destroy_with_frame_len((*batch)[i]);
    }
    WS_CHECK(i == count);
}

static void enqueue_raw(ts_queue *q, int fd, const char *text) {
    ts_enqueue(q, message_create_raw(fd, text, strlen(text), 0));
}

static void test_output_queue_keeps_raw_and_encoded_in_one_ordered_batch(void) {
    ts_queue q;
    ts_queue_init(&q);
    int fd = new_test_fd();
    int other = new_test_fd();
    /* client pipelined CONNECT with the upgrade: 101 (raw, bare fd) then CONNECTED (encoded fd) */
    enqueue_raw(&q, fd, "HTTP/1.1 101");
    ts_enqueue(&q, message_create_with_frame_len(session_set_encoded(fd), "CONNECTED"));
    ts_enqueue(&q, message_create_with_frame_len(session_set_encoded(other), "MESSAGE"));
    enqueue_raw(&q, fd, "CLOSE");

    output_batch_messages batch;
    WS_CHECK(take_batch(&q, &batch) == fd);
    const char *const expected[] = {"HTTP/1.1 101", "CONNECTED", "CLOSE"};
    check_batch_contents(&batch, expected, 3);
    output_queue_release_fd(&q, fd);

    WS_CHECK(take_batch(&q, &batch) == other);
    const char *const expected_other[] = {"MESSAGE"};
    check_batch_contents(&batch, expected_other, 1);
    output_queue_release_fd(&q, other);
    ts_queue_free(&q);
}

static void test_output_queue_one_writer_per_fd(void) {
    ts_queue q;
    ts_queue_init(&q);
    int fd = new_test_fd();
    int other = new_test_fd();
    output_batch_messages batch;

    ts_enqueue(&q, message_create_with_frame_len(session_set_encoded(fd), "ERROR"));
    WS_CHECK(take_batch(&q, &batch) == fd); /* a writer now owns fd */
    const char *const first[] = {"ERROR"};
    check_batch_contents(&batch, first, 1);

    /* later output for the same fd must wait until that writer is done */
    enqueue_raw(&q, fd, "CLOSE");
    ts_enqueue(&q, message_create_with_frame_len(session_set_encoded(other), "MESSAGE"));
    WS_CHECK(take_batch(&q, &batch) == other);
    const char *const second[] = {"MESSAGE"};
    check_batch_contents(&batch, second, 1);
    output_queue_release_fd(&q, other);

    output_queue_release_fd(&q, fd);
    WS_CHECK(take_batch(&q, &batch) == fd);
    const char *const third[] = {"CLOSE"};
    check_batch_contents(&batch, third, 1);
    output_queue_release_fd(&q, fd);
    ts_queue_free(&q);
}

/* ---------------------------------------------------------------------------
 * Suite
 * ------------------------------------------------------------------------- */
int run_websocket_tests(void) {
    session_storage_init();
    ws_init_buffer();

    printf("WebSocket decoder:\n");
    WS_RUN(test_decode_small_frame);
    WS_RUN(test_decode_byte_by_byte);
    WS_RUN(test_decode_small_frame_split_at_every_boundary);
    WS_RUN(test_decode_two_small_frames_in_one_read);
    WS_RUN(test_decode_frames_and_partial_tail);
    WS_RUN(test_decode_16bit_length);
    WS_RUN(test_decode_64bit_length);
    WS_RUN(test_message_without_nul_is_terminated);
    WS_RUN(test_heartbeat_is_not_a_message);
    WS_RUN(test_binary_frame_policy);
    WS_RUN(test_ping_does_not_wedge_connection);
    WS_RUN(test_unknown_opcode_is_consumed);
    WS_RUN(test_reject_unmasked_frame);
    WS_RUN(test_reject_reserved_bits);
    WS_RUN(test_reject_bad_control_frames);
    WS_RUN(test_reject_too_large_frame);
    WS_RUN(test_fuzz_random_bytes_always_progress);
    WS_RUN(test_fragmented_message);

    printf("WebSocket control frames:\n");
    WS_RUN(test_ping_gets_pong);
    WS_RUN(test_empty_ping_gets_empty_pong);
    WS_RUN(test_pong_is_ignored);
    WS_RUN(test_close_is_echoed);
    WS_RUN(test_close_without_code_is_echoed_empty);
    WS_RUN(test_messages_before_close_are_delivered);

    printf("Connection transport state:\n");
    WS_RUN(test_route_raw_stomp_stays_raw);
    WS_RUN(test_route_upgrade_then_frames);
    WS_RUN(test_route_empty_read_keeps_new);

    printf("WebSocket fragmentation:\n");
    WS_RUN(test_fragments_with_ping_between);
    WS_RUN(test_fragmented_binary_message);
    WS_RUN(test_fragmented_message_too_big);
    WS_RUN(test_continuation_without_start_rejected);
    WS_RUN(test_new_message_inside_fragmented_rejected);

    printf("Output path:\n");
    signal(SIGPIPE, SIG_IGN); /* as the server does */
    WS_RUN(test_output_larger_than_buffer_to_slow_reader);
    WS_RUN(test_output_gives_up_after_timeout);
    WS_RUN(test_output_drops_trickling_reader);
    WS_RUN(test_output_keeps_reader_above_min_rate);
    WS_RUN(test_output_peer_closed_fails);

    printf("Output queue:\n");
    WS_RUN(test_output_queue_keeps_raw_and_encoded_in_one_ordered_batch);
    WS_RUN(test_output_queue_one_writer_per_fd);

    printf("Output messages:\n");
    WS_RUN(test_raw_output_message_is_written_verbatim);
    WS_RUN(test_frame_len_matches_encoded_size);

    printf("WebSocket frame codec:\n");
    WS_RUN(test_codec_decode_all_length_encodings);
    WS_RUN(test_codec_masking);
    WS_RUN(test_codec_frame_too_large);
    WS_RUN(test_codec_encode_length_boundaries);
    WS_RUN(test_codec_encode_control);

    printf("WebSocket handshake:\n");
    WS_RUN(test_accept_key_rfc_sample);
    WS_RUN(test_handshake_valid);
    WS_RUN(test_handshake_mixed_case_headers);
    WS_RUN(test_handshake_wrong_version);
    WS_RUN(test_handshake_missing_key);
    WS_RUN(test_handshake_bad_requests);
    WS_RUN(test_handshake_oversize_request);
    WS_RUN(test_handshake_subprotocol);
    WS_RUN(test_handshake_origin_and_host_policy);
    WS_RUN(test_handshake_request_start_detection);
    WS_RUN(test_handshake_split_request);
    WS_RUN(test_handshake_failure_clears_buffer);

    ws_deinit_buffer();
    session_storage_dispose();

    printf("WebSocket tests: %d run, %d failed\n", ws_tests_run, ws_tests_failed);
    return ws_tests_failed;
}
