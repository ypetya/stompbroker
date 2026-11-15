/*
 * STOMP layer unit tests: raw byte buffers in, frames out.
 *
 * Portable (no epoll): links the parser, the stomp.c handlers, pub/sub and the
 * output path, so it runs on Linux and macOS. Unity-build style like main.c.
 *
 * Entry point: int run_stomp_tests(void) -> number of failed tests.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/emalloc.c"
#include "lib/general_list.c"
#include "lib/queue.c"
#include "lib/thread_safe_queue.c"
#include "lib/clone_str.c"
#include "lib/associative_array.c"
#include "parse_args.c"

#include "websocket/main.h"
#include "stomp/main.h"
#include "server/data/message/with_timestamp.c"
#include "server/data/message/with_payload_length.c"
#include "server/data/session_storage_simple.c"

/* ---------------------------------------------------------------------------
 * Minimal non-aborting test framework (same style as test_websocket.c)
 * ------------------------------------------------------------------------- */
static int st_checks_failed = 0;
static int st_tests_run = 0;
static int st_tests_failed = 0;
static const char *st_current_test = "";

#define ST_CHECK(cond) do { \
    if (!(cond)) { \
        st_checks_failed++; \
        printf("    check failed (%s:%d) %s: %s\n", __FILE__, __LINE__, st_current_test, #cond); \
    } \
} while (0)

#define ST_RUN(fn) do { \
    int failed_before = st_checks_failed; \
    st_current_test = #fn; \
    stomp_test_setup(); \
    fn(); \
    stomp_test_teardown(); \
    st_tests_run++; \
    if (st_checks_failed != failed_before) st_tests_failed++; \
    printf("  %s %s\n", st_checks_failed == failed_before ? "PASS" : "FAIL", #fn); \
} while (0)

/* ---------------------------------------------------------------------------
 * Broker fixture: the queues the reader thread would own
 * ------------------------------------------------------------------------- */
static ts_queue st_input_queue;
static ts_queue st_output_queue;
static queue st_stale_queue;

static void stomp_test_setup(void) {
    stomp_app_config *config = config_get_config();
    config->ttl = 0;
    config->max_stale_queue_size = 1000;
    ts_queue_init(&st_input_queue);
    ts_queue_init(&st_output_queue);
    queue_init(&st_stale_queue);
    stomp_start();
}

static void drop_output(void) {
    message_with_frame_len *m;
    while ((m = dequeue(&st_output_queue.q)) != NULL) message_destroy_with_frame_len(m);
}

static void stomp_test_teardown(void) {
    drop_output();
    stomp_stop(&st_stale_queue);
    ts_queue_free(&st_input_queue);
    ts_queue_free(&st_output_queue);
    stomp_session_init();
}

/** Processes one complete frame (without the terminating NUL) received on fd. */
static void receive(int fd, const char *frame) {
    message_with_timestamp *m = message_create_with_timestamp(fd, (char *) frame, 0);
    stomp_process(&st_input_queue, &st_stale_queue, &st_output_queue, m);
    message_destroy_with_timestamp(m);
}

/** Next queued output frame (caller destroys it), NULL if none. */
static message_with_frame_len *next_output(void) {
    return dequeue(&st_output_queue.q);
}

static int output_count(void) {
    return st_output_queue.q.size;
}

static int starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/** Pops the next output and checks that it starts with prefix. */
static int expect_output(const char *prefix) {
    message_with_frame_len *m = next_output();
    int ok = m != NULL && starts_with(m->content, prefix);
    if (!ok) printf("    expected %.20s..., got: %.60s\n", prefix, m ? m->content : "(nothing)");
    if (m) message_destroy_with_frame_len(m);
    return ok;
}

static void connect_client(int fd) {
    receive(fd, "CONNECT\naccept-version:1.2\nhost:localhost\n\n");
    ST_CHECK(expect_output("CONNECTED\n"));
}

/* ---------------------------------------------------------------------------
 * Harness tests
 * ------------------------------------------------------------------------- */
static void test_parse_connect_headers(void) {
    message_with_timestamp *m = message_create_with_timestamp(5,
            "SUBSCRIBE\nid:7\ndestination:/a\nreceipt:r1\n\n", 0);
    parsed_message *pm = parse_message(m);
    ST_CHECK(pm->command == FRM_SUBSCRIBE_ID);
    ST_CHECK(pm->id && strcmp(pm->id, "7") == 0);
    ST_CHECK(pm->topic && strcmp(pm->topic, "/a") == 0);
    ST_CHECK(pm->receipt_id && strcmp(pm->receipt_id, "r1") == 0);
    free_parsed_message(pm);
    message_destroy_with_timestamp(m);
}

static void test_connect_replies_connected(void) {
    connect_client(5);
    ST_CHECK(output_count() == 0);
}

static void test_send_requires_connection(void) {
    receive(5, "SEND\ndestination:/a\n\nhello");
    ST_CHECK(expect_output("ERROR\n"));
}

static void test_pubsub_round_trip(void) {
    connect_client(5);
    connect_client(6);
    receive(5, "SUBSCRIBE\nid:1\ndestination:/topic/a\n\n");
    receive(6, "SEND\ndestination:/topic/a\n\nhello");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && m->fd == 5 && starts_with(m->content, "MESSAGE\n"));
    ST_CHECK(m != NULL && strstr(m->content, "\nsubscription:1\n") != NULL);
    ST_CHECK(m != NULL && strstr(m->content, "\n\nhello") != NULL);
    if (m) message_destroy_with_frame_len(m);
}

static void test_receipt_is_sent(void) {
    connect_client(5);
    receive(5, "SUBSCRIBE\nid:1\ndestination:/a\nreceipt:r-1\n\n");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && starts_with(m->content, "RECEIPT\n") && strstr(m->content, "receipt-id:r-1\n"));
    if (m) message_destroy_with_frame_len(m);
}

/* ---------------------------------------------------------------------------
 * Malformed input (gap 1: remote segfault)
 * ------------------------------------------------------------------------- */
static void check_parse_error(const char *frame) {
    message_with_timestamp *m = message_create_with_timestamp(5, (char *) frame, 0);
    parsed_message *pm = parse_message(m);
    ST_CHECK(pm->command == -1);
    ST_CHECK(pm->error != NULL);
    if (pm->command != -1) printf("    accepted malformed frame %s\n", frame);
    free_parsed_message(pm);
    message_destroy_with_timestamp(m);
}

static void test_parse_rejects_truncated_frames(void) {
    check_parse_error("SEND\ndestination:/x\n"); /* headers end without a blank line */
    check_parse_error("SEND\ndestination:/x");   /* header line without EOL */
    check_parse_error("SEND");                   /* no EOL after the command */
    check_parse_error("");                       /* empty frame */
    check_parse_error("SEND\ndestination\n\n");  /* header without colon */
    check_parse_error("SEND\r\ndestination:/x\r\n"); /* CRLF variant */
}

static void test_headers_without_blank_line_get_error(void) {
    connect_client(5);
    receive(5, "SEND\ndestination:/x\n");
    ST_CHECK(expect_output("ERROR\n"));
    receive(5, "SEND\ndestination\n\nbody");
    ST_CHECK(expect_output("ERROR\n"));
}

/** Deterministic pseudo random numbers (xorshift32): reproducible fuzz runs. */
static unsigned int fuzz_state = 2463534242u;

static unsigned int fuzz_next(void) {
    fuzz_state ^= fuzz_state << 13;
    fuzz_state ^= fuzz_state >> 17;
    fuzz_state ^= fuzz_state << 5;
    return fuzz_state;
}

static const char *FUZZ_TOKENS[] = {
    "SEND", "SUBSCRIBE", "UNSUBSCRIBE", "CONNECT", "STOMP", "DISCONNECT", "DIAG", "SENDX",
    "\n", "\n", "\r\n", "\r", ":", "::", "destination", "id", "receipt", "content-length",
    "accept-version", "1.2", "/t", "/t/*", "#q", "0", "5", "999999", "x", "", " ", "\\c"
};

/** 10k random and truncated frames through parser and handlers: must never crash (ASan). */
static void test_fuzz_random_and_truncated_frames(void) {
    const char *valid[] = {
        "SUBSCRIBE\nid:1\ndestination:/t/*\nreceipt:r\n\n",
        "SEND\ndestination:/t/a\ncontent-length:5\n\nhello",
        "UNSUBSCRIBE\nid:1\n\n",
        "DIAG\n\nsession",
        "DISCONNECT\nreceipt:9\n\n",
        "CONNECT\naccept-version:1.2\n\n"
    };
    size_t n_tokens = sizeof (FUZZ_TOKENS) / sizeof (FUZZ_TOKENS[0]);
    char frame[512];
    connect_client(5);
    for (int i = 0; i < 10000; i++) {
        if (i % 2 == 0) { /* truncation of a valid frame */
            const char *v = valid[fuzz_next() % 6];
            size_t cut = fuzz_next() % (strlen(v) + 1);
            memcpy(frame, v, cut);
            frame[cut] = '\0';
        } else { /* random token soup */
            size_t used = 0;
            int tokens = (int) (fuzz_next() % 12);
            frame[0] = '\0';
            for (int t = 0; t < tokens; t++) {
                const char *tok = FUZZ_TOKENS[fuzz_next() % n_tokens];
                size_t tl = strlen(tok);
                if (used + tl + 1 >= sizeof (frame)) break;
                memcpy(frame + used, tok, tl);
                used += tl;
                frame[used] = '\0';
            }
        }
        receive(5 + (int) (fuzz_next() % 3), frame);
        drop_output();
    }
    ST_CHECK(1); /* reaching this point without a sanitizer report is the test */
}

/* ---------------------------------------------------------------------------
 * Strict command and header matching (gap 7)
 * ------------------------------------------------------------------------- */
static parsed_message *parse_str(const char *frame) {
    return parse_frame(frame, strlen(frame), 0);
}

static void test_command_must_match_whole_token(void) {
    const char *invalid[] = {"SENDX\ndestination:/a\n\n", "CONNECTED\n\n", "STOMPY\n\n",
        "DIAGNOSTIC\n\nsession", "SUBSCRIBEX\nid:1\ndestination:/a\n\n", "send\n\n", " SEND\n\n"};
    for (size_t i = 0; i < sizeof (invalid) / sizeof (invalid[0]); i++) {
        parsed_message *pm = parse_str(invalid[i]);
        ST_CHECK(pm->command == -1 && pm->error != NULL);
        free_parsed_message(pm);
    }
    parsed_message *pm = parse_str("SEND\r\ndestination:/a\r\n\r\n"); /* CRLF still fine */
    ST_CHECK(pm->command == FRM_SEND_ID && pm->topic && strcmp(pm->topic, "/a") == 0);
    free_parsed_message(pm);
}

static void test_header_names_must_match_whole_token(void) {
    parsed_message *pm = parse_str("SUBSCRIBE\nidx:1\ndestinationx:/a\nreceipts:r\n\n");
    ST_CHECK(pm->command == FRM_SUBSCRIBE_ID);
    ST_CHECK(pm->id == NULL && pm->topic == NULL && pm->receipt_id == NULL);
    free_parsed_message(pm);
}

static void test_repeated_header_first_wins(void) {
    parsed_message *pm = parse_str("SEND\ndestination:/first\ndestination:/second\nx:1\nx:2\n\n");
    ST_CHECK(pm->topic && strcmp(pm->topic, "/first") == 0);
    aa_item *x = aa_get(pm->headers->root, "x");
    ST_CHECK(x && strcmp(x->value, "1") == 0);
    free_parsed_message(pm);
}

static void test_prefixed_command_and_header_get_error(void) {
    connect_client(5);
    receive(5, "SENDX\ndestination:/a\n\nbody");
    ST_CHECK(expect_output("ERROR\n"));
    receive(5, "SEND\ndestinationx:/a\n\nbody");
    ST_CHECK(expect_output("ERROR\n"));
}

int run_stomp_tests(void) {
    printf("STOMP tests\n");
    session_storage_init();

    ST_RUN(test_parse_connect_headers);
    ST_RUN(test_connect_replies_connected);
    ST_RUN(test_send_requires_connection);
    ST_RUN(test_pubsub_round_trip);
    ST_RUN(test_receipt_is_sent);
    ST_RUN(test_parse_rejects_truncated_frames);
    ST_RUN(test_headers_without_blank_line_get_error);
    ST_RUN(test_fuzz_random_and_truncated_frames);
    ST_RUN(test_command_must_match_whole_token);
    ST_RUN(test_header_names_must_match_whole_token);
    ST_RUN(test_repeated_header_first_wins);
    ST_RUN(test_prefixed_command_and_header_get_error);

    session_storage_dispose();
    printf("STOMP tests: %d run, %d failed\n", st_tests_run, st_tests_failed);
    return st_tests_failed;
}
