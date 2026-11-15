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
    return fd;
}

/* ---------------------------------------------------------------------------
 * Decoder tests (gaps 1-4), written against the current decoder contract.
 * ------------------------------------------------------------------------- */
static void test_decode_small_frame(void) {
    unsigned char frame[64];
    size_t n = make_frame(frame, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    WS_CHECK(n == 13);

    int fd = new_test_fd();
    char *out = NULL;
    size_t out_len = 0;
    int status = ws_input_filter_dataframe(fd, (char *) frame, n, &out, &out_len);
    WS_CHECK(status == WS_COMPLETE_DATAFRAME);
    WS_CHECK(out != NULL && out_len == 6 && memcmp(out, "SEND\n\n", 6) == 0);
    free(out);
}

static void test_decode_byte_by_byte(void) {
    char *body = repeat_char('x', 200);
    unsigned char frame[512];
    size_t n = make_frame(frame, 1, 1, body, 201, 1); /* include body's NUL */

    int fd = new_test_fd();
    int completes = 0;
    for (size_t i = 0; i < n; i++) {
        char *out = NULL;
        size_t out_len = 0;
        int status = ws_input_filter_dataframe(fd, (char *) &frame[i], 1, &out, &out_len);
        if (status == WS_COMPLETE_DATAFRAME) {
            completes++;
            WS_CHECK(i == n - 1);
            WS_CHECK(out_len == 200 && memcmp(out, body, 200) == 0);
        }
        free(out);
    }
    WS_CHECK(completes == 1);
    free(body);
}

static void test_decode_small_frame_split_at_every_boundary(void) {
    unsigned char frame[64];
    size_t n = make_frame(frame, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    for (size_t k = 1; k < n; k++) {
        int fd = new_test_fd();
        char *out = NULL;
        size_t out_len = 0;
        int first = ws_input_filter_dataframe(fd, (char *) frame, k, &out, &out_len);
        WS_CHECK(first == WS_INCOMPLETE_DATAFRAME && out == NULL);
        free(out);
        out = NULL;
        int second = ws_input_filter_dataframe(fd, (char *) frame + k, n - k, &out, &out_len);
        WS_CHECK(second == WS_COMPLETE_DATAFRAME);
        WS_CHECK(out != NULL && out_len == 6 && memcmp(out, "SEND\n\n", 6) == 0);
        free(out);
    }
}

static void test_decode_two_small_frames_in_one_read(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);
    n += make_frame(buf + n, 1, 1, SMALL_STOMP, SMALL_STOMP_LEN, 1);

    int fd = new_test_fd();
    char *out = NULL;
    size_t out_len = 0;
    ws_input_filter_dataframe(fd, (char *) buf, n, &out, &out_len);
    /* Both frames: "SEND\n\n\0SEND\n\n" (last NUL is excluded from the length). */
    WS_CHECK(out != NULL && out_len == 13 && memcmp(out, "SEND\n\n\0SEND\n\n", 13) == 0);
    free(out);
}

static void test_decode_16bit_length(void) {
    char *body = repeat_char('m', 300);
    unsigned char frame[400];
    size_t n = make_frame(frame, 1, 1, body, 301, 1);

    int fd = new_test_fd();
    char *out = NULL;
    size_t out_len = 0;
    int status = ws_input_filter_dataframe(fd, (char *) frame, n, &out, &out_len);
    WS_CHECK(status == WS_COMPLETE_DATAFRAME);
    WS_CHECK(out_len == 300 && memcmp(out, body, 300) == 0);
    free(out);
    free(body);
}

static void test_decode_64bit_length(void) {
    size_t len = 70000;
    char *body = repeat_char('L', len);
    unsigned char *frame = malloc(len + 32);
    size_t n = make_frame(frame, 1, 1, body, len + 1, 1);

    int fd = new_test_fd();
    char *out = NULL;
    size_t out_len = 0;
    int status = ws_input_filter_dataframe(fd, (char *) frame, n, &out, &out_len);
    WS_CHECK(status == WS_COMPLETE_DATAFRAME);
    WS_CHECK(out_len == len && memcmp(out, body, len) == 0);
    free(out);
    free(frame);
    free(body);
}

static void test_ping_does_not_wedge_connection(void) {
    unsigned char buf[128];
    size_t n = make_frame(buf, 1, 9, "hello", 5, 1);
    n += make_frame(buf + n, 1, 1, "SEND\ndestination:/a\n\n", 22, 1);

    int fd = new_test_fd();
    char *out = NULL;
    size_t out_len = 0;
    ws_input_filter_dataframe(fd, (char *) buf, n, &out, &out_len);
    WS_CHECK(out != NULL && out_len == 21 && memcmp(out, "SEND\ndestination:/a\n\n", 21) == 0);
    free(out);
}

static void test_unknown_opcode_is_consumed(void) {
    unsigned char buf[64];
    size_t n = make_frame(buf, 1, 3, "abcdefghijklmnop", 16, 1);

    int fd = new_test_fd();
    char *out = NULL;
    size_t out_len = 0;
    ws_input_filter_dataframe(fd, (char *) buf, n, &out, &out_len);
    buffer_item *left = ws_buffer_find(fd);
    WS_CHECK(left == NULL || left->received_len == 0);
    free(out);
}

static void test_fragmented_message(void) {
    unsigned char buf[128];
    size_t n = make_frame(buf, 0, 1, "SEND\ndesti", 10, 1);
    n += make_frame(buf + n, 0, 0, "nation:/a\n", 10, 1);
    n += make_frame(buf + n, 1, 0, "\nbody", 6, 1); /* incl. NUL */

    int fd = new_test_fd();
    char *out = NULL;
    size_t out_len = 0;
    ws_input_filter_dataframe(fd, (char *) buf, n, &out, &out_len);
    WS_CHECK(out != NULL && out_len == 25 && memcmp(out, "SEND\ndestination:/a\n\nbody", 25) == 0);
    free(out);
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

/** Runs the old filter and returns the queued response (or NULL). Caller frees. */
static char *old_handshake(int fd, const char *request) {
    ts_queue q;
    ts_queue_init(&q);
    message_with_timestamp *m = message_create_with_timestamp(fd, (char *) request, 0);
    ws_input_filter_handshake(&q, m);
    message_destroy_with_timestamp(m);
    char *resp = NULL;
    if (q.q.size > 0) {
        message_with_frame_len *r = ts_dequeue(&q);
        resp = strdup(r->content);
        message_destroy_with_frame_len(r);
    }
    return resp;
}

static void test_handshake_valid(void) {
    char *resp = old_handshake(new_test_fd(),
            "GET /chat HTTP/1.1\r\nHost: server.example.com\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Key: " RFC_SAMPLE_KEY "\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n");
    WS_CHECK(resp != NULL && strncmp(resp, "HTTP/1.1 101", 12) == 0);
    WS_CHECK(resp != NULL && strstr(resp, "Sec-WebSocket-Accept: " RFC_SAMPLE_ACCEPT "\r\n") != NULL);
    free(resp);
}

static void test_handshake_mixed_case_headers(void) {
    char *resp = old_handshake(new_test_fd(),
            "GET /chat HTTP/1.1\r\nhost: server.example.com\r\nupgrade: WebSocket\r\n"
            "connection: keep-alive, Upgrade\r\nsec-websocket-key: " RFC_SAMPLE_KEY "\r\n"
            "SEC-WEBSOCKET-VERSION: 13\r\n\r\n");
    WS_CHECK(resp != NULL && strncmp(resp, "HTTP/1.1 101", 12) == 0);
    WS_CHECK(resp != NULL && strstr(resp, RFC_SAMPLE_ACCEPT) != NULL);
    free(resp);
}

static void test_handshake_wrong_version(void) {
    char *resp = old_handshake(new_test_fd(),
            "GET /chat HTTP/1.1\r\nHost: h\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Key: " RFC_SAMPLE_KEY "\r\n"
            "Sec-WebSocket-Version: 8\r\n\r\n");
    WS_CHECK(resp != NULL && strncmp(resp, "HTTP/1.1 426", 12) == 0);
    free(resp);
}

static void test_handshake_missing_key(void) {
    char *resp = old_handshake(new_test_fd(),
            "GET /chat HTTP/1.1\r\nHost: h\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n\r\n");
    WS_CHECK(resp != NULL && strncmp(resp, "HTTP/1.1 400", 12) == 0);
    free(resp);
}

static void test_handshake_split_request(void) {
    int fd = new_test_fd();
    char *first = old_handshake(fd, "GET /chat HTTP/1.1\r\nHost: h\r\nUpgrade: websocket\r\nConne");
    char *second = old_handshake(fd, "ction: Upgrade\r\nSec-WebSocket-Key: " RFC_SAMPLE_KEY "\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n");
    WS_CHECK(first == NULL);
    WS_CHECK(second != NULL && strncmp(second, "HTTP/1.1 101", 12) == 0);
    free(first);
    free(second);
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
    WS_RUN(test_decode_16bit_length);
    WS_RUN(test_decode_64bit_length);
    WS_RUN(test_ping_does_not_wedge_connection);
    WS_RUN(test_unknown_opcode_is_consumed);
    WS_RUN(test_fragmented_message);

    printf("WebSocket handshake:\n");
    WS_RUN(test_accept_key_rfc_sample);
    WS_RUN(test_handshake_valid);
    WS_RUN(test_handshake_mixed_case_headers);
    WS_RUN(test_handshake_wrong_version);
    WS_RUN(test_handshake_missing_key);
    WS_RUN(test_handshake_split_request);

    ws_deinit_buffer();
    session_storage_dispose();

    printf("WebSocket tests: %d run, %d failed\n", ws_tests_run, ws_tests_failed);
    return ws_tests_failed;
}
