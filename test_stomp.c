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
#include "server/data/cleanup.c"
#include "server/threads/output_batch.c"
#include "server/threads/output_queue.c"
#include "server/threads/writer_thread.c"

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

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
    config->max_stale_queue_bytes = DEFAULT_STALE_QUEUE_BYTES;
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

/* ---------------------------------------------------------------------------
 * Frame reassembly (gap 2)
 * ------------------------------------------------------------------------- */
#define MAX_COLLECTED 16

typedef struct collected_frames_st {
    int count;
    char *frames[MAX_COLLECTED];
    size_t lens[MAX_COLLECTED];
} collected_frames;

static void collect_frame(void *ctx, const char *frame, size_t len) {
    collected_frames *c = ctx;
    if (c->count >= MAX_COLLECTED) return;
    c->frames[c->count] = emalloc(len + 1);
    memcpy(c->frames[c->count], frame, len);
    c->lens[c->count++] = len;
}

static void collected_free(collected_frames *c) {
    for (int i = 0; i < c->count; i++) free(c->frames[i]);
    memset(c, 0, sizeof (*c));
}

static int collected_is(const collected_frames *c, int i, const char *frame, size_t len) {
    return i < c->count && c->lens[i] == len && memcmp(c->frames[i], frame, len) == 0;
}

static const char TWO_FRAMES[] = "\n\r\nSEND\ndestination:/a\n\nfirst\0\nSUBSCRIBE\nid:1\ndestination:/b\n\n\0";
#define TWO_FRAMES_LEN (sizeof (TWO_FRAMES) - 1)
#define FIRST_FRAME "SEND\ndestination:/a\n\nfirst"
#define SECOND_FRAME "SUBSCRIBE\nid:1\ndestination:/b\n\n"

static void check_two_frames(const collected_frames *c) {
    ST_CHECK(c->count == 2);
    ST_CHECK(collected_is(c, 0, FIRST_FRAME, strlen(FIRST_FRAME)));
    ST_CHECK(collected_is(c, 1, SECOND_FRAME, strlen(SECOND_FRAME)));
}

static void test_framing_two_frames_in_one_read(void) {
    stomp_input_buffer in = {0};
    collected_frames c = {0};
    const char *error = NULL;
    ST_CHECK(stomp_input_feed(&in, TWO_FRAMES, TWO_FRAMES_LEN, 1000, collect_frame, &c, &error) == 0);
    check_two_frames(&c);
    ST_CHECK(in.len == 0);
    collected_free(&c);
    stomp_input_reset(&in);
}

static void test_framing_split_at_every_offset(void) {
    for (size_t cut = 0; cut <= TWO_FRAMES_LEN; cut++) {
        stomp_input_buffer in = {0};
        collected_frames c = {0};
        const char *error = NULL;
        ST_CHECK(stomp_input_feed(&in, TWO_FRAMES, cut, 1000, collect_frame, &c, &error) == 0);
        ST_CHECK(stomp_input_feed(&in, TWO_FRAMES + cut, TWO_FRAMES_LEN - cut, 1000, collect_frame, &c, &error) == 0);
        check_two_frames(&c);
        ST_CHECK(in.len == 0);
        collected_free(&c);
        stomp_input_reset(&in);
    }
}

static void test_framing_byte_by_byte(void) {
    stomp_input_buffer in = {0};
    collected_frames c = {0};
    const char *error = NULL;
    for (size_t i = 0; i < TWO_FRAMES_LEN; i++)
        ST_CHECK(stomp_input_feed(&in, TWO_FRAMES + i, 1, 1000, collect_frame, &c, &error) == 0);
    check_two_frames(&c);
    collected_free(&c);
    stomp_input_reset(&in);
}

static void test_framing_heartbeats_only(void) {
    stomp_input_buffer in = {0};
    collected_frames c = {0};
    const char *error = NULL;
    ST_CHECK(stomp_input_feed(&in, "\n\r\n\n", 4, 1000, collect_frame, &c, &error) == 0);
    ST_CHECK(c.count == 0 && in.len == 0);
    stomp_input_reset(&in);
}

static void test_framing_content_length_split_at_every_offset(void) {
    const char frame[] = "SEND\ndestination:/a\ncontent-length:3\n\nabc\0";
    size_t len = sizeof (frame) - 1;
    for (size_t cut = 0; cut <= len; cut++) {
        stomp_input_buffer in = {0};
        collected_frames c = {0};
        const char *error = NULL;
        stomp_input_feed(&in, frame, cut, 1000, collect_frame, &c, &error);
        stomp_input_feed(&in, frame + cut, len - cut, 1000, collect_frame, &c, &error);
        ST_CHECK(c.count == 1 && collected_is(&c, 0, frame, len - 1));
        collected_free(&c);
        stomp_input_reset(&in);
    }
}

static void test_framing_large_frame_in_read_sized_chunks(void) {
    size_t body_len = 20000, chunk = 16000; /* 20 KB SEND, input_buffer_size=16000 */
    const char *head = "SEND\ndestination:/big\n\n";
    size_t len = strlen(head) + body_len + 1;
    char *data = emalloc(len);
    memcpy(data, head, strlen(head));
    memset(data + strlen(head), 'B', body_len);
    stomp_input_buffer in = {0};
    collected_frames c = {0};
    const char *error = NULL;
    for (size_t off = 0; off < len; off += chunk)
        ST_CHECK(stomp_input_feed(&in, data + off, off + chunk > len ? len - off : chunk,
            DEFAULT_MAX_FRAME_SIZE, collect_frame, &c, &error) == 0);
    ST_CHECK(c.count == 1 && collected_is(&c, 0, data, len - 1));
    collected_free(&c);
    stomp_input_reset(&in);
    free(data);
}

static void test_framing_too_large_is_an_error(void) {
    stomp_input_buffer in = {0};
    collected_frames c = {0};
    const char *error = NULL;
    char data[64];
    memset(data, 'x', sizeof (data));
    memcpy(data, "SEND\n\n", 6);
    ST_CHECK(stomp_input_feed(&in, data, 20, 32, collect_frame, &c, &error) == 0); /* not yet */
    ST_CHECK(stomp_input_feed(&in, data + 20, 20, 32, collect_frame, &c, &error) == -1);
    ST_CHECK(error != NULL && c.count == 0 && in.len == 0 && in.data == NULL);
    /* a declared content-length above the cap fails at once */
    error = NULL;
    const char *declared = "SEND\ncontent-length:100\n\n";
    ST_CHECK(stomp_input_feed(&in, declared, strlen(declared), 32, collect_frame, &c, &error) == -1);
    ST_CHECK(error != NULL);
    stomp_input_reset(&in);
}

static void test_framing_unterminated_headers_reach_the_parser(void) {
    stomp_input_buffer in = {0};
    collected_frames c = {0};
    const char *error = NULL;
    const char data[] = "SEND\ndestination:/x\n\0"; /* gap 1 frame: complete, parser rejects it */
    ST_CHECK(stomp_input_feed(&in, data, sizeof (data) - 1, 1000, collect_frame, &c, &error) == 0);
    ST_CHECK(c.count == 1 && collected_is(&c, 0, data, sizeof (data) - 2));
    collected_free(&c);
    stomp_input_reset(&in);
}

static void test_framing_error_becomes_error_frame(void) {
    message_with_timestamp *m = message_create_with_timestamp(5, "", 0);
    m->error = "Frame too large!";
    stomp_process(&st_input_queue, &st_stale_queue, &st_output_queue, m);
    message_destroy_with_timestamp(m);
    message_with_frame_len *out = next_output();
    ST_CHECK(out != NULL && starts_with(out->content, "ERROR\n") && strstr(out->content, "Frame too large!"));
    ST_CHECK(out != NULL && out->close_after);
    if (out) message_destroy_with_frame_len(out);
}

/* ---------------------------------------------------------------------------
 * content-length (gap 9)
 * ------------------------------------------------------------------------- */
static void receive_bytes(int fd, const char *frame, size_t len) {
    message_with_timestamp *m = message_create_with_timestamp_len(fd, frame, len, 0);
    stomp_process(&st_input_queue, &st_stale_queue, &st_output_queue, m);
    message_destroy_with_timestamp(m);
}

static int contains_bytes(const char *hay, size_t hay_len, const char *needle, size_t needle_len) {
    for (size_t i = 0; i + needle_len <= hay_len; i++)
        if (memcmp(hay + i, needle, needle_len) == 0) return 1;
    return 0;
}

static void test_parse_body_with_nul_and_content_length(void) {
    const char frame[] = "SEND\ndestination:/a\ncontent-length:3\n\na\0b";
    parsed_message *pm = parse_frame(frame, sizeof (frame) - 1, 0);
    ST_CHECK(pm->command == FRM_SEND_ID && pm->error == NULL);
    ST_CHECK(pm->body_len == 3 && memcmp(pm->message_body, "a\0b", 3) == 0);
    free_parsed_message(pm);
}

static void test_parse_rejects_content_length_mismatch(void) {
    const char *frames[] = {
        "SEND\ndestination:/a\ncontent-length:5\n\nabc",
        "SEND\ndestination:/a\ncontent-length:1\n\nabc",
        "SEND\ndestination:/a\ncontent-length:x\n\nabc"
    };
    for (int i = 0; i < 3; i++) {
        parsed_message *pm = parse_frame(frames[i], strlen(frames[i]), 0);
        ST_CHECK(pm->command == -1 && pm->error != NULL);
        free_parsed_message(pm);
    }
    const char nul_without_length[] = "SEND\ndestination:/a\n\na\0b";
    parsed_message *pm = parse_frame(nul_without_length, sizeof (nul_without_length) - 1, 0);
    ST_CHECK(pm->command == -1 && pm->error != NULL);
    free_parsed_message(pm);
}

static void test_body_with_nul_round_trips(void) {
    connect_client(5);
    receive(5, "SUBSCRIBE\nid:1\ndestination:/bin\n\n");
    const char frame[] = "SEND\ndestination:/bin\ncontent-length:5\n\nx\0y\0z";
    connect_client(6);
    receive_bytes(6, frame, sizeof (frame) - 1);
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && m->fd == 5 && starts_with(m->content, "MESSAGE\n"));
    if (m != NULL) {
        ST_CHECK(strstr(m->content, "\ncontent-length:5\n") != NULL);
        ST_CHECK(m->content_len >= 7 && memcmp(m->content + m->content_len - 7, "\n\nx\0y\0z", 7) == 0);
        size_t wire = ws_output_filter(m); /* raw TCP: frame + terminating NUL */
        ST_CHECK(wire == m->content_len + 1 && m->content[m->content_len] == '\0');
        message_destroy_with_frame_len(m);
    }
}

static void test_body_with_nul_over_websocket(void) {
    const char body[] = "MESSAGE\ncontent-length:3\n\na\0b";
    message_with_frame_len *m = message_create_with_frame_len_n(session_set_encoded(9), body, sizeof (body) - 1);
    size_t predicted = m->frame_len;
    size_t len = ws_output_filter(m);
    ST_CHECK(len == predicted && len == 2 + sizeof (body)); /* header + bytes + NUL */
    ST_CHECK(contains_bytes(m->content, len, "a\0b\0", 4));
    message_destroy_with_frame_len(m);
}

/* ---------------------------------------------------------------------------
 * Protocol fixes (gaps 5, 6, 8, 13)
 * ------------------------------------------------------------------------- */
static void test_receipt_frame_bytes(void) {
    connect_client(5);
    receive(5, "SUBSCRIBE\nid:1\ndestination:/a\nreceipt:x\n\n");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL);
    if (m == NULL) return;
    size_t wire = ws_output_filter(m);
    const char expected[] = "RECEIPT\nreceipt-id:x\n\n"; /* + its NUL */
    ST_CHECK(wire == sizeof (expected) && memcmp(m->content, expected, sizeof (expected)) == 0);
    if (wire != sizeof (expected)) printf("    got %s\n", m->content);
    message_destroy_with_frame_len(m);
}

static void test_unsubscribe_needs_only_id(void) {
    connect_client(5);
    receive(5, "SUBSCRIBE\nid:1\ndestination:/a\n\n");
    receive(5, "SUBSCRIBE\nid:2\ndestination:/b\n\n");
    ST_CHECK(pubsub_size() == 2);
    receive(5, "UNSUBSCRIBE\nid:1\nreceipt:u1\n\n");
    ST_CHECK(expect_output("RECEIPT\nreceipt-id:u1\n"));
    ST_CHECK(pubsub_size() == 1);
    receive(6, "CONNECT\naccept-version:1.2\n\n");
    drop_output();
    receive(6, "SEND\ndestination:/a\n\nnobody listens");
    ST_CHECK(output_count() == 0);
    receive(5, "UNSUBSCRIBE\nreceipt:u2\n\n");
    ST_CHECK(expect_output("ERROR\n")); /* id is still required */
}

static void test_duplicate_subscription_id_rejected(void) {
    connect_client(5);
    connect_client(6);
    receive(6, "SUBSCRIBE\nid:1\ndestination:/a\n\n"); /* the same id on another connection is fine */
    ST_CHECK(output_count() == 0);
    receive(5, "SUBSCRIBE\nid:1\ndestination:/a\n\n");
    receive(5, "SUBSCRIBE\nid:1\ndestination:/a/*\n\n");
    ST_CHECK(expect_output("ERROR\n"));
    ST_CHECK(pubsub_size() == 1); /* ERROR ended the session of fd 5 */
}

static void test_duplicate_subscription_id_no_double_delivery(void) {
    connect_client(5);
    receive(5, "SUBSCRIBE\nid:1\ndestination:/a\n\n");
    receive(5, "SUBSCRIBE\nid:1\ndestination:/a/*\n\n");
    drop_output();
    connect_client(6);
    receive(6, "SEND\ndestination:/a\n\nonce");
    ST_CHECK(output_count() == 0); /* never twice; the erroneous session gets nothing at all */
}

static void test_http_prefixed_output_keeps_nul(void) {
    const char frame[] = "MESSAGE\ndestination:/a\n\nHTTP/1.1 200 OK";
    message_with_frame_len *m = message_create_with_frame_len(7, (char *) frame + 24); /* body only: "HTTP..." */
    ST_CHECK(ws_output_filter(m) == strlen(frame + 24) + 1);
    message_destroy_with_frame_len(m);
    m = message_create_with_frame_len(7, "http lowercase too");
    ST_CHECK(ws_output_filter(m) == strlen("http lowercase too") + 1);
    message_destroy_with_frame_len(m);
}

static void test_diag_frame_has_blank_line(void) {
    receive(5, "DIAG\n\npubsub-size");
    ST_CHECK(expect_output("DIAG\ncontent-type:text/plain\npubsub-size:0\n\n"));
}

/* ---------------------------------------------------------------------------
 * Safe connection close across threads (gap 12)
 * ------------------------------------------------------------------------- */
static int socket_pair(int sv[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return -1;
    fcntl(sv[0], F_SETFL, O_NONBLOCK);
    return 0;
}

static void enqueue_for_session(ts_queue *q, int fd, const char *text, unsigned int generation) {
    message_with_frame_len *m = message_create_with_frame_len(fd, (char *) text);
    m->generation = generation;
    ts_enqueue(q, m);
}

static output_batch test_batch(int fd, char *buffer, size_t capacity) {
    output_batch b = {.fd = 0, .buffer = buffer, .capacity = capacity, .used = 0,
        .timeout_ms = 200, .min_bytes = capacity, .stall = output_queue_stall_of(fd)};
    return b;
}

/** One writer round: take a batch, write it, hand the fd back. */
static void writer_round(ts_queue *q) {
    char buffer[1000];
    message_with_frame_len *batch[MESSAGES_BATCH_SIZE] = {NULL};
    int fd = output_queue_take_batch(&batch, q, sizeof (buffer));
    output_batch b = test_batch(fd, buffer, sizeof (buffer));
    writer_send_batch(&batch, &b);
    output_queue_release_fd(q, fd);
}

/** Reads what is available within ~200 ms. */
static size_t read_some(int fd, char *buf, size_t cap) {
    size_t got = 0;
    for (int i = 0; i < 20 && got < cap; i++) {
        ssize_t n = recv(fd, buf + got, cap - got, MSG_DONTWAIT);
        if (n > 0) got += (size_t) n;
        else if (n == 0) break;
        else usleep(10000);
    }
    return got;
}

typedef struct closer_st {
    int fd;
    ts_queue *input_queue;
    ts_queue *output_queue;
    volatile int done;
} closer;

static void *closer_thread(void *arg) {
    closer *c = arg;
    connection_close(c->fd, c->input_queue, c->output_queue);
    c->done = 1;
    return NULL;
}

static void test_close_waits_for_writer_and_new_session_gets_no_old_bytes(void) {
    ts_queue in, out;
    ts_queue_init(&in);
    ts_queue_init(&out);
    int old_pair[2], new_pair[2];
    ST_CHECK(socket_pair(old_pair) == 0 && socket_pair(new_pair) == 0);
    int fd = old_pair[0];
    session_storage_add_new(fd);
    unsigned int old_generation = session_storage_generation(fd);

    /* per-fd state of the old session */
    ws_buffer_add(fd, "abc", 3);
    output_queue_stall_of(fd)->waited_ms = 1234;
    stomp_input_feed(stomp_input_of(fd), "SEND\n", 5, 100, collect_frame, NULL, &(const char *){NULL});

    /* a writer holds a batch of the old session */
    enqueue_for_session(&out, fd, "OLD", old_generation);
    char buffer[1000];
    message_with_frame_len *batch[MESSAGES_BATCH_SIZE] = {NULL};
    ST_CHECK(output_queue_take_batch(&batch, &out, sizeof (buffer)) == fd);

    closer c = {fd, &in, &out, 0};
    pthread_t t;
    pthread_create(&t, NULL, closer_thread, &c);
    usleep(100000);
    ST_CHECK(c.done == 0); /* close waits while the writer owns the fd */

    output_batch b = test_batch(fd, buffer, sizeof (buffer));
    writer_send_batch(&batch, &b); /* fails or succeeds: the old peer is being shut down */
    output_queue_release_fd(&out, fd);
    pthread_join(t, NULL);
    ST_CHECK(c.done == 1);
    ST_CHECK(session_storage_get(fd) == 0);
    ST_CHECK(session_storage_generation(fd) != old_generation);

    /* the purge for the old session is queued for the reader */
    message_with_timestamp *purge = dequeue(&in.q);
    ST_CHECK(purge != NULL && session_is_cmd_purge(purge->fd) && session_without_flags(purge->fd) == fd);
    if (purge) message_destroy_with_timestamp(purge);

    /* output the reader produced late for the old session */
    enqueue_for_session(&out, fd, "STALE", old_generation);

    /* a new client gets the same fd number */
    ST_CHECK(dup2(new_pair[0], fd) == fd);
    close(new_pair[0]);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    session_storage_add_new(fd);
    enqueue_for_session(&out, fd, "NEW", session_storage_generation(fd));

    ST_CHECK(ws_buffer_find(fd) == NULL); /* fresh WebSocket buffers */
    ST_CHECK(output_queue_stall_of(fd)->waited_ms == 0 && output_queue_stall_of(fd)->bytes == 0); /* fresh budget */
    ST_CHECK(stomp_input_of(fd)->len == 0);

    writer_round(&out);
    char got[64] = {0};
    size_t n = read_some(new_pair[1], got, sizeof (got));
    ST_CHECK(n == 4 && memcmp(got, "NEW\0", 4) == 0);
    if (n != 4) printf("    new client got %zu bytes: %.*s\n", n, (int) n, got);

    connection_close(fd, &in, &out);
    while ((purge = dequeue(&in.q)) != NULL) message_destroy_with_timestamp(purge);
    close(old_pair[1]);
    close(new_pair[1]);
    ts_queue_free(&in);
    ts_queue_free(&out);
}

static void test_writer_requests_close_with_shutdown(void) {
    ts_queue out;
    ts_queue_init(&out);
    int sv[2];
    ST_CHECK(socket_pair(sv) == 0);
    session_storage_add_new(sv[0]);
    message_with_frame_len *m = message_create_with_frame_len(sv[0], "ERROR\n\nbye");
    m->generation = session_storage_generation(sv[0]);
    m->close_after = 1;
    ts_enqueue(&out, m);
    writer_round(&out);

    ST_CHECK(fcntl(sv[0], F_GETFD) != -1); /* the fd number stays reserved for the epoll thread */
    ST_CHECK(session_storage_get(sv[0]) != 0);
    char got[64];
    size_t n = read_some(sv[1], got, sizeof (got));
    ST_CHECK(n == strlen("ERROR\n\nbye") + 1);
    ST_CHECK(recv(sv[1], got, sizeof (got), MSG_DONTWAIT) == 0); /* then EOF */

    session_storage_lock();
    session_storage_remove(sv[0]);
    session_storage_unlock();
    close(sv[0]);
    close(sv[1]);
    ts_queue_free(&out);
}

static void test_replies_carry_the_session_generation(void) {
    message_with_timestamp *m = message_create_with_timestamp(5, "CONNECT\naccept-version:1.2\n\n", 0);
    m->generation = 42;
    stomp_process(&st_input_queue, &st_stale_queue, &st_output_queue, m);
    message_destroy_with_timestamp(m);
    message_with_frame_len *out = next_output();
    ST_CHECK(out != NULL && out->generation == 42);
    if (out) message_destroy_with_frame_len(out);

    m = message_create_with_timestamp(5, "SUBSCRIBE\nid:1\ndestination:/g\n\n", 0);
    m->generation = 42;
    stomp_process(&st_input_queue, &st_stale_queue, &st_output_queue, m);
    message_destroy_with_timestamp(m);
    connect_client(6);
    receive(6, "SEND\ndestination:/g\n\nx");
    out = next_output();
    ST_CHECK(out != NULL && out->fd == 5 && out->generation == 42); /* MESSAGE: generation of the subscriber */
    if (out) message_destroy_with_frame_len(out);
}

/* ---------------------------------------------------------------------------
 * DISCONNECT, ERROR and version negotiation (gaps 4, 10)
 * ------------------------------------------------------------------------- */
static void test_disconnect_with_receipt_then_close(void) {
    connect_client(5);
    receive(5, "SUBSCRIBE\nid:1\ndestination:/d\n\n");
    receive(5, "DISCONNECT\nreceipt:77\n\n");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && strcmp(m->content, "RECEIPT\nreceipt-id:77\n\n") == 0 && m->close_after);
    if (m) message_destroy_with_frame_len(m);
    ST_CHECK(pubsub_size() == 0);
    ST_CHECK(stomp_session_is_connected(5) == 0);
}

static void test_disconnect_without_receipt_closes(void) {
    connect_client(5);
    receive(5, "DISCONNECT\n\n");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && m->close_after && m->raw && m->content_len == 0); /* nothing to send, just close */
    if (m) message_destroy_with_frame_len(m);
}

static void test_error_has_receipt_id_and_closes(void) {
    connect_client(5);
    receive(5, "SUBSCRIBE\nid:1\ndestination:/e\n\n");
    receive(5, "SUBSCRIBE\nid:1\ndestination:/e\nreceipt:dup\n\n");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && starts_with(m->content, "ERROR\n") && strstr(m->content, "\nreceipt-id:dup\n"));
    ST_CHECK(m != NULL && m->close_after);
    ST_CHECK(m != NULL && strstr(m->content, "\nmessage:Subscription id is already in use!\n"));
    if (m) message_destroy_with_frame_len(m);
    /* the session is over: no more deliveries, frames are refused */
    ST_CHECK(pubsub_size() == 0 && stomp_session_is_connected(5) == 0);
}

static void test_every_error_closes(void) {
    const char *bad[] = {"INVALID\n\n", "SEND\n\nno destination", "SUBSCRIBE\nid:1\n\n", "CONNECT\naccept-version:1.2\n\n"};
    for (int i = 0; i < 4; i++) {
        connect_client(5);
        receive(5, bad[i]);
        message_with_frame_len *m = next_output();
        ST_CHECK(m != NULL && starts_with(m->content, "ERROR\n") && m->close_after);
        if (m) message_destroy_with_frame_len(m);
        drop_output();
        stomp_session_set_connected(5, 0);
    }
}

static void check_version(const char *accept_version_header, const char *expected) {
    char frame[128];
    snprintf(frame, sizeof (frame), "CONNECT\n%s\n", accept_version_header);
    receive(5, frame);
    message_with_frame_len *m = next_output();
    char version_line[32];
    snprintf(version_line, sizeof (version_line), "\nversion:%s\n", expected);
    ST_CHECK(m != NULL && starts_with(m->content, "CONNECTED\n") && strstr(m->content, version_line));
    if (m == NULL || !strstr(m->content, version_line))
        printf("    %s -> %s\n", accept_version_header, m ? m->content : "(nothing)");
    if (m) message_destroy_with_frame_len(m);
    stomp_session_set_connected(5, 0);
}

static void test_accept_version_negotiation(void) {
    check_version("accept-version:1.2\n", "1.2");
    check_version("accept-version:1.1\n", "1.1");
    check_version("accept-version:1.0,1.1\n", "1.1");
    check_version("accept-version:1.0, 1.1 ,1.2\n", "1.2");
    check_version("host:x\n", "1.0"); /* no accept-version: a STOMP 1.0 client */
}

static void test_unsupported_version_is_rejected(void) {
    receive(5, "CONNECT\naccept-version:2.0,1.3\n\n");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && starts_with(m->content, "ERROR\n") && m->close_after);
    ST_CHECK(m != NULL && strstr(m->content, "\nversion:1.0,1.1,1.2\n"));
    if (m) message_destroy_with_frame_len(m);
    ST_CHECK(stomp_session_is_connected(5) == 0);
}

/* ---------------------------------------------------------------------------
 * TTL stale queue (gap 3)
 * ------------------------------------------------------------------------- */
static void receive_now(int fd, const char *frame) {
    message_with_timestamp *m = message_create_with_timestamp(fd, (char *) frame, message_timestamp_now());
    stomp_process(&st_input_queue, &st_stale_queue, &st_output_queue, m);
    message_destroy_with_timestamp(m);
}

static void test_ttl_message_waits_for_subscriber(void) {
    ttl = 5000 * 1000; /* TTL=5000 ms */
    connect_client(6);
    receive_now(6, "SEND\ndestination:/ttl/a\ncustom:1\n\nkept");
    ST_CHECK(output_count() == 0 && st_stale_queue.size == 1);

    connect_client(5);
    receive_now(5, "SUBSCRIBE\nid:s1\ndestination:/ttl/*\n\n");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && m->fd == 5 && starts_with(m->content, "MESSAGE\n"));
    if (m != NULL) {
        ST_CHECK(strstr(m->content, "\ndestination:/ttl/a\n") != NULL); /* the message's destination */
        ST_CHECK(strstr(m->content, "\nsubscription:s1\n") != NULL && strstr(m->content, "\ncustom:1\n") != NULL);
        ST_CHECK(m->content_len > 6 && memcmp(m->content + m->content_len - 6, "\n\nkept", 6) == 0);
        message_destroy_with_frame_len(m);
    }
    ST_CHECK(st_stale_queue.size == 0 && stale_queue_bytes() == 0);
}

static void test_ttl_zero_stores_nothing(void) {
    connect_client(6);
    receive_now(6, "SEND\ndestination:/ttl/a\n\nlost");
    ST_CHECK(st_stale_queue.size == 0);
}

static void test_ttl_expired_messages_are_dropped(void) {
    ttl = 1000; /* 1 ms */
    connect_client(6);
    receive_now(6, "SEND\ndestination:/ttl/a\n\nold");
    ST_CHECK(st_stale_queue.size == 1);
    usleep(20000);
    connect_client(5);
    receive_now(5, "SUBSCRIBE\nid:s1\ndestination:/ttl/a\n\n");
    ST_CHECK(output_count() == 0 && st_stale_queue.size == 0 && stale_queue_bytes() == 0);
    /* expired messages also leave when a new one is stored */
    receive_now(6, "SEND\ndestination:/ttl/b\n\nold");
    usleep(20000);
    receive_now(6, "SEND\ndestination:/ttl/c\n\nnew");
    ST_CHECK(st_stale_queue.size == 1);
}

static void test_ttl_store_is_bounded(void) {
    ttl = 5000 * 1000;
    stale_queue_max_size = 2;
    connect_client(6);
    receive_now(6, "SEND\ndestination:/ttl/1\n\na");
    receive_now(6, "SEND\ndestination:/ttl/2\n\nb");
    receive_now(6, "SEND\ndestination:/ttl/3\n\nc"); /* dropped: count limit */
    ST_CHECK(st_stale_queue.size == 2);
    stale_queue_max_size = 1000;
    stale_queue_max_bytes = stale_queue_bytes() + 300; /* room for a small one */
    char large[600];
    snprintf(large, sizeof (large), "SEND\ndestination:/ttl/4\n\n%0400d", 0); /* body too large for the byte limit */
    receive_now(6, large);
    ST_CHECK(st_stale_queue.size == 2);
    receive_now(6, "SEND\ndestination:/t\n\nok");
    ST_CHECK(st_stale_queue.size == 3);
    stale_queue_max_bytes = DEFAULT_STALE_QUEUE_BYTES;
}

static void test_message_headers_forwarded_from_send(void) {
    connect_client(5);
    receive(5, "SUBSCRIBE\nid:s\ndestination:/h\n\n");
    connect_client(6);
    receive(6, "SEND\ndestination:/h\ncontent-type:application/json\nreceipt:r9\nx-custom:1\n\n{}");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && m->fd == 5 && starts_with(m->content, "MESSAGE\n"));
    if (m != NULL) {
        ST_CHECK(strstr(m->content, "\ncontent-type:application/json\n") != NULL); /* the client's type is kept */
        ST_CHECK(strstr(m->content, "\nx-custom:1\n") != NULL && strstr(m->content, "\nmessage-id:") != NULL);
        ST_CHECK(strstr(m->content, "receipt") == NULL); /* the sender's receipt request is not forwarded */
        message_destroy_with_frame_len(m);
    }
    ST_CHECK(expect_output("RECEIPT\nreceipt-id:r9\n\n"));
}

/* ---------------------------------------------------------------------------
 * Code review F1 follow-ups
 * ------------------------------------------------------------------------- */
typedef struct claimer_st {
    ts_queue *q;
    int fd;
    volatile int done;
} claimer;

static void *claimer_thread(void *arg) {
    claimer *c = arg;
    output_queue_claim_fd(c->q, c->fd);
    c->done = 1;
    return NULL;
}

static void *writer_round_thread(void *arg) {
    writer_round(arg);
    return NULL;
}

/* review 1: a close waiting for a busy fd must not swallow the wake-up meant for writers */
static void test_close_waiting_on_busy_fd_does_not_stall_other_output(void) {
    ts_queue out;
    ts_queue_init(&out);
    int busy[2], other[2];
    ST_CHECK(socket_pair(busy) == 0 && socket_pair(other) == 0);
    session_storage_add_new(busy[0]);
    session_storage_add_new(other[0]);

    /* a writer is busy with the first connection */
    enqueue_for_session(&out, busy[0], "BUSY", session_storage_generation(busy[0]));
    message_with_frame_len *batch[MESSAGES_BATCH_SIZE] = {NULL};
    ST_CHECK(output_queue_take_batch(&batch, &out, 1000) == busy[0]);

    /* the epoll thread waits to close it; an idle writer waits for output (in this order) */
    claimer c = {&out, busy[0], 0};
    pthread_t closer_t, writer_t;
    pthread_create(&closer_t, NULL, claimer_thread, &c);
    usleep(50000);
    pthread_create(&writer_t, NULL, writer_round_thread, &out);
    usleep(50000);

    /* output for another connection must go out while the close keeps waiting */
    enqueue_for_session(&out, other[0], "OTHER", session_storage_generation(other[0]));
    char got[16] = {0};
    size_t n = read_some(other[1], got, sizeof (got));
    ST_CHECK(n == 6 && memcmp(got, "OTHER\0", 6) == 0);
    ST_CHECK(c.done == 0);

    for (int i = 0; batch[i] != NULL; i++) message_destroy_with_frame_len(batch[i]);
    output_queue_release_fd(&out, busy[0]); /* the close may go now */
    pthread_join(closer_t, NULL);
    pthread_join(writer_t, NULL);
    ST_CHECK(c.done == 1);
    output_queue_release_fd(&out, busy[0]);

    session_storage_lock();
    session_storage_remove(busy[0]);
    session_storage_remove(other[0]);
    session_storage_unlock();
    close(busy[0]); close(busy[1]); close(other[0]); close(other[1]);
    ts_queue_free(&out);
}

/* review 3: nothing is written after ERROR, even when it is in the same batch */
static void test_nothing_is_written_after_error(void) {
    ts_queue out;
    ts_queue_init(&out);
    int sv[2];
    ST_CHECK(socket_pair(sv) == 0);
    session_storage_add_new(sv[0]);
    unsigned int generation = session_storage_generation(sv[0]);

    message_with_frame_len *error = message_create_with_frame_len(sv[0], "ERROR\n\nbye");
    error->generation = generation;
    error->close_after = 1;
    ts_enqueue(&out, error);
    enqueue_for_session(&out, sv[0], "MESSAGE\n\nlate", generation);
    ST_CHECK(out.q.size == 2);
    writer_round(&out); /* both in one batch */
    ST_CHECK(out.q.size == 0);

    char got[64];
    size_t n = read_some(sv[1], got, sizeof (got));
    ST_CHECK(n == strlen("ERROR\n\nbye") + 1 && memcmp(got, "ERROR\n\nbye", n) == 0);
    ST_CHECK(recv(sv[1], got, sizeof (got), MSG_DONTWAIT) == 0); /* then EOF */

    session_storage_lock();
    session_storage_remove(sv[0]);
    session_storage_unlock();
    close(sv[0]);
    close(sv[1]);
    ts_queue_free(&out);
}

/* review 4: stored messages are charged for their headers too */
static void test_ttl_store_charges_headers(void) {
    ttl = 5000 * 1000;
    connect_client(6);
    receive_now(6, "SEND\ndestination:/ttl/h\nx-small:1\n\n");
    ST_CHECK(st_stale_queue.size == 1);
    ST_CHECK(stale_queue_bytes() >= sizeof (parsed_message) + sizeof (aa_item) * 2 + strlen("x-small1destination/ttl/h"));

    stale_queue_max_bytes = stale_queue_bytes() + 1000;
    size_t header_len = 4000;
    char *frame = emalloc(header_len + 64);
    int head = snprintf(frame, 64, "SEND\ndestination:/ttl/h\nx-big:");
    memset(frame + head, 'h', header_len);
    strcpy(frame + head + header_len, "\n\n"); /* empty body, 4 KB of headers */
    receive_now(6, frame);
    free(frame);
    ST_CHECK(st_stale_queue.size == 1); /* refused: over the byte limit */
    stale_queue_max_bytes = DEFAULT_STALE_QUEUE_BYTES;
}

/* review 6: WebSocket sessions end with a close frame */
static int is_ws_close(message_with_frame_len *m, uint16_t code) {
    return m != NULL && m->raw && m->close_after && m->content_len == 4
            && (unsigned char) m->content[0] == 0x88 && m->content[1] == 2
            && (unsigned char) m->content[2] == (code >> 8) && (unsigned char) m->content[3] == (code & 0xff);
}

static void test_websocket_disconnect_sends_close_frame(void) {
    int ws = session_set_encoded(5);
    connect_client(ws);
    receive(ws, "DISCONNECT\nreceipt:9\n\n");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && strcmp(m->content, "RECEIPT\nreceipt-id:9\n\n") == 0 && !m->close_after);
    if (m) message_destroy_with_frame_len(m);
    m = next_output();
    ST_CHECK(is_ws_close(m, WS_CLOSE_NORMAL));
    if (m) message_destroy_with_frame_len(m);

    connect_client(ws);
    receive(ws, "DISCONNECT\n\n"); /* without receipt: only the close frame */
    m = next_output();
    ST_CHECK(is_ws_close(m, WS_CLOSE_NORMAL) && output_count() == 0);
    if (m) message_destroy_with_frame_len(m);
}

static void test_websocket_error_sends_close_frame(void) {
    int ws = session_set_encoded(5);
    connect_client(ws);
    receive(ws, "SEND\n\nno destination");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && starts_with(m->content, "ERROR\n") && !m->close_after);
    if (m) message_destroy_with_frame_len(m);
    m = next_output();
    ST_CHECK(is_ws_close(m, WS_CLOSE_POLICY_VIOLATION));
    if (m) message_destroy_with_frame_len(m);

    /* a framing error ends the same way */
    message_with_timestamp *in = message_create_with_timestamp(ws, "", 0);
    in->error = "Frame too large!";
    stomp_process(&st_input_queue, &st_stale_queue, &st_output_queue, in);
    message_destroy_with_timestamp(in);
    ST_CHECK(expect_output("ERROR\n"));
    m = next_output();
    ST_CHECK(is_ws_close(m, WS_CLOSE_POLICY_VIOLATION));
    if (m) message_destroy_with_frame_len(m);
}

/* review 5: an existing session is never refused when the limit is reached */
static void test_existing_session_survives_full_storage(void) {
    session_storage_lock();
    ST_CHECK(session_storage_add_new(7) == 7);
    int saved = number_of_clients;
    number_of_clients = MAX_NUMBER_OF_CONNECTIONS;
    ST_CHECK(session_storage_add_new(7) == SESSION_IS_IN_USE);
    ST_CHECK(session_storage_add_new(8) == MAX_SESSION_NUMBER_EXCEEDED);
    number_of_clients = saved;
    session_storage_remove(7);
    session_storage_unlock();
}

/* fds above 14 bits must not alias lower fds, and flags must not leak into the fd */
static void test_high_fds_do_not_alias(void) {
    ST_CHECK(session_without_flags(16384) == 16384);
    ST_CHECK(session_without_flags(MAX_NUMBER_OF_CONNECTIONS) == MAX_NUMBER_OF_CONNECTIONS);
    int flagged = session_set_cmd_purge(session_set_encoded(MAX_NUMBER_OF_CONNECTIONS));
    ST_CHECK(session_without_flags(flagged) == MAX_NUMBER_OF_CONNECTIONS);
    ST_CHECK(session_is_encoded(flagged) && session_is_cmd_purge(flagged));
    ST_CHECK(stomp_input_of(16384) != stomp_input_of(0));
}

/* review 7: a frame split over many reads is rescanned only when it can be complete */
static void test_framing_waits_without_rescanning(void) {
    stomp_input_buffer in = {0};
    collected_frames c = {0};
    const char *error = NULL;
    const char *head = "SEND\ndestination:/a\ncontent-length:10\n\n";
    ST_CHECK(stomp_input_feed(&in, head, strlen(head), 1000, collect_frame, &c, &error) == 0);
    ST_CHECK(in.wait_len == strlen(head) + 11 && !in.wait_nul);
    ST_CHECK(stomp_input_feed(&in, "01234", 5, 1000, collect_frame, &c, &error) == 0 && c.count == 0);
    ST_CHECK(stomp_input_feed(&in, "56789\0SEND\n\nx", 13, 1000, collect_frame, &c, &error) == 0);
    ST_CHECK(c.count == 1 && collected_is(&c, 0, "SEND\ndestination:/a\ncontent-length:10\n\n0123456789", strlen(head) + 10));
    ST_CHECK(in.len == 7 && in.wait_nul && in.wait_len == 0); /* "SEND\n\nx" waits for its NUL */
    ST_CHECK(stomp_input_feed(&in, "yz", 2, 1000, collect_frame, &c, &error) == 0 && c.count == 1);
    ST_CHECK(stomp_input_feed(&in, "\0", 1, 1000, collect_frame, &c, &error) == 0);
    ST_CHECK(c.count == 2 && collected_is(&c, 1, "SEND\n\nxyz", 9) && in.len == 0 && in.data == NULL);
    collected_free(&c);
    stomp_input_reset(&in);
}

static void test_framing_large_content_length_frame_in_chunks(void) {
    size_t body_len = DEFAULT_MAX_FRAME_SIZE - 100, chunk = 16000;
    char head[64];
    int head_len = snprintf(head, sizeof (head), "SEND\ndestination:/big\ncontent-length:%zu\n\n", body_len);
    size_t len = (size_t) head_len + body_len + 1;
    char *data = emalloc(len);
    memcpy(data, head, (size_t) head_len);
    memset(data + head_len, 'B', body_len);
    data[head_len + 7] = '\0'; /* NUL in the body is fine with content-length */
    stomp_input_buffer in = {0};
    collected_frames c = {0};
    const char *error = NULL;
    for (size_t off = 0; off < len; off += chunk)
        ST_CHECK(stomp_input_feed(&in, data + off, off + chunk > len ? len - off : chunk,
            DEFAULT_MAX_FRAME_SIZE, collect_frame, &c, &error) == 0);
    ST_CHECK(c.count == 1 && collected_is(&c, 0, data, len - 1));
    collected_free(&c);
    stomp_input_reset(&in);
    free(data);
}

/* review 8: '*' matches an empty rest, for live and for stored messages alike */
static void test_wildcard_matches_empty_rest(void) {
    ST_CHECK(pubsub_topic_matches("/topic/*", "/topic/"));
    ST_CHECK(pubsub_topic_matches("/topic/*", "/topic/a"));
    ST_CHECK(!pubsub_topic_matches("/topic/*", "/topic"));
    ST_CHECK(!pubsub_topic_matches("/topic/a", "/topic/"));

    connect_client(5);
    receive(5, "SUBSCRIBE\nid:live\ndestination:/w/*\n\n");
    connect_client(6);
    receive(6, "SEND\ndestination:/w/\n\nlive");
    message_with_frame_len *m = next_output();
    ST_CHECK(m != NULL && m->fd == 5 && starts_with(m->content, "MESSAGE\n"));
    if (m) message_destroy_with_frame_len(m);

    ttl = 5000 * 1000;
    receive_now(6, "SEND\ndestination:/s/\n\nstored");
    ST_CHECK(st_stale_queue.size == 1);
    receive_now(5, "SUBSCRIBE\nid:stored\ndestination:/s/*\n\n");
    m = next_output();
    ST_CHECK(m != NULL && m->fd == 5 && strstr(m->content, "\ndestination:/s/\n") != NULL);
    if (m) message_destroy_with_frame_len(m);
    ST_CHECK(st_stale_queue.size == 0);
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
    ST_RUN(test_framing_two_frames_in_one_read);
    ST_RUN(test_framing_split_at_every_offset);
    ST_RUN(test_framing_byte_by_byte);
    ST_RUN(test_framing_heartbeats_only);
    ST_RUN(test_framing_content_length_split_at_every_offset);
    ST_RUN(test_framing_large_frame_in_read_sized_chunks);
    ST_RUN(test_framing_too_large_is_an_error);
    ST_RUN(test_framing_unterminated_headers_reach_the_parser);
    ST_RUN(test_framing_error_becomes_error_frame);
    ST_RUN(test_parse_body_with_nul_and_content_length);
    ST_RUN(test_parse_rejects_content_length_mismatch);
    ST_RUN(test_body_with_nul_round_trips);
    ST_RUN(test_body_with_nul_over_websocket);
    ST_RUN(test_receipt_frame_bytes);
    ST_RUN(test_unsubscribe_needs_only_id);
    ST_RUN(test_duplicate_subscription_id_rejected);
    ST_RUN(test_duplicate_subscription_id_no_double_delivery);
    ST_RUN(test_http_prefixed_output_keeps_nul);
    ST_RUN(test_diag_frame_has_blank_line);
    ST_RUN(test_close_waits_for_writer_and_new_session_gets_no_old_bytes);
    ST_RUN(test_writer_requests_close_with_shutdown);
    ST_RUN(test_replies_carry_the_session_generation);
    ST_RUN(test_disconnect_with_receipt_then_close);
    ST_RUN(test_disconnect_without_receipt_closes);
    ST_RUN(test_error_has_receipt_id_and_closes);
    ST_RUN(test_every_error_closes);
    ST_RUN(test_accept_version_negotiation);
    ST_RUN(test_unsupported_version_is_rejected);
    ST_RUN(test_ttl_message_waits_for_subscriber);
    ST_RUN(test_ttl_zero_stores_nothing);
    ST_RUN(test_ttl_expired_messages_are_dropped);
    ST_RUN(test_ttl_store_is_bounded);
    ST_RUN(test_message_headers_forwarded_from_send);
    ST_RUN(test_close_waiting_on_busy_fd_does_not_stall_other_output);
    ST_RUN(test_nothing_is_written_after_error);
    ST_RUN(test_ttl_store_charges_headers);
    ST_RUN(test_websocket_disconnect_sends_close_frame);
    ST_RUN(test_websocket_error_sends_close_frame);
    ST_RUN(test_existing_session_survives_full_storage);
    ST_RUN(test_framing_waits_without_rescanning);
    ST_RUN(test_framing_large_content_length_frame_in_chunks);
    ST_RUN(test_wildcard_matches_empty_rest);
    ST_RUN(test_high_fds_do_not_alias);

    session_storage_dispose();
    printf("STOMP tests: %d run, %d failed\n", st_tests_run, st_tests_failed);
    return st_tests_failed;
}
