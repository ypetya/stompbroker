#include "parser.h"
#include <stdio.h>
#include <string.h>
#include "../lib/clone_str.h"
#include "../lib/emalloc.h"

/*
 * STOMP 1.2 frame parser.
 *
 *   frame = command EOL *(header EOL) EOL *OCTET
 *
 * The input is a single frame without its terminating NUL. Every access is
 * bounded by the frame length: malformed input yields command == -1 and a
 * static reason in pm->error, never an out of bounds read.
 */

/** One line of the frame, without its EOL ("\n" or "\r\n"). */
typedef struct frame_line_st {
    const char *p;
    size_t len;
    size_t next; // offset of the following line
    int complete; // the line is terminated by '\n'
} frame_line;

static frame_line read_line(const char *frame, size_t len, size_t offset) {
    frame_line l = {frame + offset, 0, len, 0};
    const char *nl = offset < len ? memchr(frame + offset, '\n', len - offset) : NULL;
    if (nl == NULL) {
        l.len = offset < len ? len - offset : 0;
        return l;
    }
    l.len = (size_t) (nl - l.p);
    l.next = (size_t) (nl - frame) + 1;
    l.complete = 1;
    if (l.len > 0 && l.p[l.len - 1] == '\r') l.len--;
    return l;
}

static char *span_dup(const char *p, size_t len) {
    char *s = emalloc(len + 1); // zeroed: s[len] == '\0'
    if (len > 0) memcpy(s, p, len);
    return s;
}

/** Whole token comparison: "SENDX" is not "SEND", "destinationx" is not "destination". */
static int token_is(const char *p, size_t len, const char *expected) {
    return len == strlen(expected) && memcmp(p, expected, len) == 0;
}

static int command_of(const char *p, size_t len) {
    if (token_is(p, len, FRM_CONNECT) || token_is(p, len, FRM_STOMP)) return FRM_CONNECT_ID;
    if (token_is(p, len, FRM_DISCONNECT)) return FRM_DISCONNECT_ID;
    if (token_is(p, len, FRM_SUBSCRIBE)) return FRM_SUBSCRIBE_ID;
    if (token_is(p, len, FRM_UNSUBSCRIBE)) return FRM_UNSUBSCRIBE_ID;
    if (token_is(p, len, FRM_SEND)) return FRM_SEND_ID;
    if (token_is(p, len, FRM_DIAGNOSTIC)) return FRM_DIAGNOSTIC_ID;
    return -1;
}

static void replace_str(char **field, const char *value) {
    free(*field);
    *field = clone_str((char *) value);
}

static void put_header(parsed_message *pm, const char *name, size_t name_len,
        const char *value, size_t value_len) {
    char *key = span_dup(name, name_len);
    if (aa_has(pm->headers->root, key)) { // STOMP 1.2: only the first of repeated headers counts
        free(key);
        return;
    }
    char *val = span_dup(value, value_len);

    if (token_is(name, name_len, "receipt")) replace_str(&pm->receipt_id, val);
    else if (token_is(name, name_len, "id")) replace_str(&pm->id, val);
    else if (token_is(name, name_len, "destination")) replace_str(&pm->topic, val);

    aa_put(pm->headers, key, val);
    free(key);
    free(val);
}

static parsed_message *parse_error(parsed_message *pm, const char *reason) {
    pm->command = -1;
    pm->error = reason;
    return pm;
}

parsed_message* parse_frame(const char *frame, size_t len, clock_t ts) {
    parsed_message* pm = emalloc(sizeof (parsed_message));
    pm->ts = ts;
    pm->headers = aa_create();

    frame_line command = read_line(frame, len, 0);
    if (!command.complete) return parse_error(pm, "Invalid frame: the command must end with an EOL!");
    pm->command = command_of(command.p, command.len);

    size_t offset = command.next;
    for (;;) {
        frame_line header = read_line(frame, len, offset);
        if (!header.complete) return parse_error(pm, "Invalid frame: headers must end with an empty line!");
        offset = header.next;
        if (header.len == 0) break; // blank line: body follows

        const char *colon = memchr(header.p, ':', header.len);
        if (colon == NULL) return parse_error(pm, "Invalid frame: header line without ':'!");
        size_t name_len = (size_t) (colon - header.p);
        put_header(pm, header.p, name_len, colon + 1, header.len - name_len - 1);
    }

    pm->message_body = span_dup(frame + offset, len - offset);

    if (pm->command < 0) return parse_error(pm, "Invalid message!");
    return pm;
}

parsed_message* parse_message(message_with_timestamp* message) {
    return parse_frame(message->content, strlen(message->content), message->ts);
}

void free_parsed_message(parsed_message* pm) {
    aa_free(pm->headers);
    free(pm->receipt_id);
    free(pm->message_body);
    free(pm->topic);
    free(pm->id);
    free(pm);
}
