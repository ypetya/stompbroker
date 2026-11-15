#include "framing.h"
#include <stdlib.h>
#include <string.h>
#include "../lib/emalloc.h"
#include "../server/data/session_storage.h"

#define CONTENT_LENGTH_HEADER "content-length:"

static int is_eol(char c) {
    return c == '\n' || c == '\r';
}

static stomp_frame_status frame_error(stomp_frame_span *span, const char *reason) {
    span->error = reason;
    span->consumed = 0;
    return STOMP_FRAME_ERROR;
}

/** Offset of the body (just after the empty line ending the headers), -1 if not within len. */
static long body_offset(const char *frame, size_t len) {
    const char *nl = memchr(frame, '\n', len); // end of the command line
    while (nl != NULL) {
        size_t line = (size_t) (nl - frame) + 1;
        if (line < len && frame[line] == '\n') return (long) line + 1;
        if (line + 1 < len && frame[line] == '\r' && frame[line + 1] == '\n') return (long) line + 2;
        nl = line < len ? memchr(frame + line, '\n', len - line) : NULL;
    }
    return -1;
}

/**
 * Value of the first content-length header between the command line and body.
 * @return -1: no such header, -2: invalid value, otherwise the length (capped above limit)
 */
static long long content_length(const char *frame, size_t body, size_t limit) {
    const char *line = memchr(frame, '\n', body);
    size_t name_len = strlen(CONTENT_LENGTH_HEADER);
    while (line != NULL && (size_t) (++line - frame) < body) {
        size_t left = body - (size_t) (line - frame);
        if (left > name_len && memcmp(line, CONTENT_LENGTH_HEADER, name_len) == 0) {
            const char *p = line + name_len;
            long long value = 0;
            int digits = 0;
            for (; p < frame + body && *p >= '0' && *p <= '9'; p++, digits++)
                if (value <= (long long) limit) value = value * 10 + (*p - '0');
            if (p < frame + body && *p == '\r') p++;
            if (digits == 0 || p >= frame + body || *p != '\n') return -2;
            return value;
        }
        line = memchr(line, '\n', left);
    }
    return -1;
}

stomp_frame_status stomp_find_frame(const char *buf, size_t len, size_t max_frame_len,
        stomp_frame_span *span) {
    memset(span, 0, sizeof (stomp_frame_span));

    size_t start = 0;
    while (start < len && is_eol(buf[start])) start++; // heart-beats
    span->start = start;
    span->consumed = start;
    if (start == len) return STOMP_FRAME_NEED_MORE;

    const char *frame = buf + start;
    size_t available = len - start;
    const char *nul = memchr(frame, '\0', available);
    size_t header_area = nul != NULL ? (size_t) (nul - frame) : available; // headers never contain NUL
    long body = body_offset(frame, header_area);

    if (body >= 0) {
        long long declared = content_length(frame, (size_t) body, max_frame_len);
        if (declared == -2) return frame_error(span, "Invalid content-length header!");
        if (declared >= 0) {
            size_t end = (size_t) body + (size_t) declared;
            if (declared > (long long) max_frame_len || end > max_frame_len)
                return frame_error(span, "Frame too large!");
            if (available <= end) return STOMP_FRAME_NEED_MORE;
            if (frame[end] != '\0') return frame_error(span, "Frame body does not match its content-length!");
            span->len = end;
            span->consumed = start + end + 1;
            return STOMP_FRAME_COMPLETE;
        }
    }

    if (nul != NULL) { // frame without content-length (or with broken headers: the parser reports it)
        span->len = (size_t) (nul - frame);
        if (span->len > max_frame_len) return frame_error(span, "Frame too large!");
        span->consumed = start + span->len + 1;
        return STOMP_FRAME_COMPLETE;
    }
    if (available > max_frame_len) return frame_error(span, "Frame too large!");
    return STOMP_FRAME_NEED_MORE;
}

void stomp_input_reset(stomp_input_buffer *in) {
    free(in->data);
    in->data = NULL;
    in->len = 0;
}

/** Keeps src[offset..src_len) as the pending bytes of the connection. */
static void keep_tail(stomp_input_buffer *in, const char *src, size_t src_len, size_t offset) {
    size_t left = src_len - offset;
    if (left == 0) {
        stomp_input_reset(in);
    } else if (src == in->data) {
        if (offset > 0) memmove(in->data, in->data + offset, left);
        in->len = left;
    } else {
        in->data = emalloc(left);
        memcpy(in->data, src + offset, left);
        in->len = left;
    }
}

int stomp_input_feed(stomp_input_buffer *in, const char *data, size_t len, size_t max_frame_len,
        stomp_frame_sink sink, void *ctx, const char **error) {
    const char *src = data;
    size_t src_len = len;
    if (in->len > 0) { // continue a partial frame
        in->data = erealloc(in->data, in->len + len);
        if (len > 0) memcpy(in->data + in->len, data, len);
        in->len += len;
        src = in->data;
        src_len = in->len;
    }

    size_t offset = 0;
    stomp_frame_span span;
    stomp_frame_status status;
    while ((status = stomp_find_frame(src + offset, src_len - offset, max_frame_len, &span)) == STOMP_FRAME_COMPLETE) {
        sink(ctx, src + offset + span.start, span.len);
        offset += span.consumed;
    }

    if (status == STOMP_FRAME_ERROR) {
        *error = span.error;
        stomp_input_reset(in);
        return -1;
    }
    keep_tail(in, src, src_len, offset + span.consumed);
    return 0;
}

static stomp_input_buffer stomp_inputs[FD_MASK + 1];

stomp_input_buffer *stomp_input_of(int fd) {
    return &stomp_inputs[fd & FD_MASK];
}
