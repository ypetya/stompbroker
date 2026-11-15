
#include "handshake.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include "../../logger.h"
#include "../../lib/emalloc.h"
#include "../buffer.h"

#include "../utils/md5.c"
#include "../utils/sha1.c"
#include "../utils/base64.c"

/**
 * We are supporting websocket RFC6455 only!
 *
 * The request is parsed line by line; header names are case-insensitive,
 * header values are trimmed, list headers (Connection, Upgrade,
 * Sec-WebSocket-Protocol) are matched token by token.
 */

/* ======================================================================
 * Accept key
 * ====================================================================== */

static const char * WEBSOCKET_MAGIC_KEY = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

char* create_accept_key(const char * client_key) {
    SHA1Context sha;
    int i, magic_len = 36, length = magic_len + strlen(client_key);
    uint32_t number;
    char key[length], sha1Key[20];
    char *acceptKey = NULL;

    memset(key, '\0', length);
    memset(sha1Key, '\0', 20);

    memcpy(key, client_key, (length - magic_len));
    memcpy(key + (length - magic_len), WEBSOCKET_MAGIC_KEY, magic_len);

    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char*) key, length);

    if (!SHA1Result(&sha)) {
        return NULL;
    }

    for (i = 0; i < 5; i++) {
        number = ntohl(sha.Message_Digest[i]);
        memcpy(sha1Key + (4 * i), (unsigned char *) &number, 4);
    }

    if (base64_encode_alloc((const char *) sha1Key, 20, &acceptKey) == 0) {

        return NULL;
    }

    return acceptKey;
}

/* ======================================================================
 * Small string helpers working on (pointer, length) spans
 * ====================================================================== */

typedef struct span_t {
    const char *p; // NULL: header absent
    size_t len;
} span;

static span span_trim(span s) {
    while (s.len > 0 && (s.p[0] == ' ' || s.p[0] == '\t')) {
        s.p++;
        s.len--;
    }
    while (s.len > 0 && (s.p[s.len - 1] == ' ' || s.p[s.len - 1] == '\t')) s.len--;
    return s;
}

static int span_ieq(span s, const char *literal) {
    return s.p != NULL && strlen(literal) == s.len && strncasecmp(s.p, literal, s.len) == 0;
}

/** Calls match(token) for each comma separated, trimmed token; stops on a hit. */
static int span_any_token(span list, int (*match)(span token, const void *ctx), const void *ctx) {
    if (list.p == NULL) return 0;
    const char *end = list.p + list.len;
    const char *start = list.p;
    for (;;) {
        const char *comma = memchr(start, ',', (size_t) (end - start));
        const char *stop = comma ? comma : end;
        span token = span_trim((span) {start, (size_t) (stop - start)});
        if (token.len > 0 && match(token, ctx)) return 1;
        if (comma == NULL) return 0;
        start = comma + 1;
    }
}

static int token_equals_literal(span token, const void *literal) {
    return span_ieq(token, (const char *) literal);
}

static int token_equals_span(span token, const void *value) {
    const span *v = value;
    return token.len == v->len && strncasecmp(token.p, v->p, v->len) == 0;
}

static int list_contains(span list, const char *literal) {
    return span_any_token(list, token_equals_literal, literal);
}

/** NULL/"" allowlist allows everything. */
static int allowlist_allows(const char *allowlist, span value) {
    if (allowlist == NULL || *allowlist == '\0') return 1;
    return span_any_token((span) {allowlist, strlen(allowlist)}, token_equals_span, &value);
}

/** Host header without the port (IPv6 literals in brackets are kept whole). */
static span host_without_port(span host) {
    const char *colon = NULL;
    if (host.len > 0 && host.p[0] == '[') {
        const char *bracket = memchr(host.p, ']', host.len);
        if (bracket) return (span) {host.p, (size_t) (bracket - host.p) + 1};
    } else {
        colon = memchr(host.p, ':', host.len);
    }
    return colon ? (span) {host.p, (size_t) (colon - host.p)} : host;
}

static const char *find_end_of_headers(const char *s, size_t len) {
    for (size_t i = 0; i + 3 < len; i++)
        if (s[i] == '\r' && s[i + 1] == '\n' && s[i + 2] == '\r' && s[i + 3] == '\n') return s + i;
    return NULL;
}

/* ======================================================================
 * Request parsing
 * ====================================================================== */

#define WS_MAX_PROTOCOL_HEADERS 8

typedef struct ws_request_t {
    span host, upgrade, connection, key, version, origin;
    span protocols[WS_MAX_PROTOCOL_HEADERS];
    int protocol_count;
} ws_request;

int ws_is_http_request_start(const char *data, size_t len) {
    static const char GET[] = "GET ";
    size_t n = len < 4 ? len : 4;
    if (memcmp(data, GET, n) != 0) return -1;
    return len >= 4 ? 1 : 0;
}

/** "GET <target> HTTP/1.x" with x >= 1 */
static int parse_request_line(span line) {
    if (line.len < 4 || memcmp(line.p, "GET ", 4) != 0) return 0;
    const char *target = line.p + 4;
    const char *end = line.p + line.len;
    const char *space = memchr(target, ' ', (size_t) (end - target));
    if (space == NULL || space == target) return 0;
    span version = {space + 1, (size_t) (end - space - 1)};
    return version.len == 8 && memcmp(version.p, "HTTP/1.", 7) == 0 && version.p[7] >= '1' && version.p[7] <= '9';
}

static void store_header(ws_request *req, span name, span value) {
    if (span_ieq(name, "Host")) req->host = value;
    else if (span_ieq(name, "Upgrade")) req->upgrade = value;
    else if (span_ieq(name, "Connection")) req->connection = value;
    else if (span_ieq(name, "Sec-WebSocket-Key")) req->key = value;
    else if (span_ieq(name, "Sec-WebSocket-Version")) req->version = value;
    else if (span_ieq(name, "Origin")) req->origin = value;
    else if (span_ieq(name, "Sec-WebSocket-Protocol") && req->protocol_count < WS_MAX_PROTOCOL_HEADERS)
        req->protocols[req->protocol_count++] = value;
}

/** @return 1 when the request line and every header line are well formed */
static int parse_request(const char *request, size_t headers_end, ws_request *req) {
    memset(req, 0, sizeof (ws_request));
    const char *cursor = request;
    const char *end = request + headers_end + 2; // keep the CRLF of the last header line
    int first = 1;

    while (cursor < end) {
        const char *crlf = NULL;
        for (const char *c = cursor; c + 1 < end; c++)
            if (c[0] == '\r' && c[1] == '\n') {
                crlf = c;
                break;
            }
        if (crlf == NULL) return 0;
        span line = {cursor, (size_t) (crlf - cursor)};
        cursor = crlf + 2;

        if (first) {
            if (!parse_request_line(line)) return 0;
            first = 0;
            continue;
        }
        const char *colon = memchr(line.p, ':', line.len);
        if (colon == NULL || colon == line.p) return 0;
        span name = {line.p, (size_t) (colon - line.p)};
        if (memchr(name.p, ' ', name.len) || memchr(name.p, '\t', name.len)) return 0; // no obs-fold / bad name
        store_header(req, name, span_trim((span) {colon + 1, (size_t) (line.p + line.len - colon - 1)}));
    }
    return !first;
}

/** 24 base64 characters decoding to 16 bytes (RFC 6455 4.1). */
static int is_valid_key(span key) {
    char decoded[18];
    size_t decoded_len = sizeof (decoded);
    return key.p != NULL && key.len == 24 &&
            base64_decode(key.p, key.len, decoded, &decoded_len) && decoded_len == 16;
}

static const char *SUPPORTED_PROTOCOLS[] = {"v12.stomp", "v11.stomp", "v10.stomp"}; // broker preference

static const char *negotiate_protocol(const ws_request *req) {
    for (size_t i = 0; i < sizeof (SUPPORTED_PROTOCOLS) / sizeof (SUPPORTED_PROTOCOLS[0]); i++)
        for (int h = 0; h < req->protocol_count; h++)
            if (list_contains(req->protocols[h], SUPPORTED_PROTOCOLS[i])) return SUPPORTED_PROTOCOLS[i];
    return NULL;
}

/* ======================================================================
 * Responses
 * ====================================================================== */

static void set_response(ws_handshake_result *result, const char *text) {
    result->response_len = strlen(text);
    result->response = emalloc(result->response_len + 1);
    memcpy(result->response, text, result->response_len);
}

static ws_handshake_status respond_error(ws_handshake_result *result, ws_handshake_status status) {
    switch (status) {
        case WS_HANDSHAKE_UPGRADE_REQUIRED:
            set_response(result, "HTTP/1.1 426 Upgrade Required\r\n"
                    "Upgrade: websocket\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "Connection: close\r\n"
                    "Content-Length: 0\r\n\r\n");
            break;
        case WS_HANDSHAKE_FORBIDDEN:
            set_response(result, "HTTP/1.1 403 Forbidden\r\n"
                    "Connection: close\r\n"
                    "Content-Length: 0\r\n\r\n");
            break;
        default:
            status = WS_HANDSHAKE_BAD_REQUEST;
            set_response(result, "HTTP/1.1 400 Bad Request\r\n"
                    "Connection: close\r\n"
                    "Content-Length: 0\r\n\r\n");
            break;
    }
    return status;
}

static ws_handshake_status respond_switching(ws_handshake_result *result, span key, const char *protocol) {
    char client_key[25];
    memcpy(client_key, key.p, key.len);
    client_key[key.len] = '\0';
    char *accept = create_accept_key(client_key);
    if (accept == NULL) return respond_error(result, WS_HANDSHAKE_BAD_REQUEST);

    char protocol_header[64] = "";
    if (protocol != NULL) {
        snprintf(protocol_header, sizeof (protocol_header), "Sec-WebSocket-Protocol: %s\r\n", protocol);
        snprintf(result->protocol, sizeof (result->protocol), "%s", protocol);
    }

    char response[256];
    snprintf(response, sizeof (response), "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "%s\r\n", accept, protocol_header);
    free(accept);
    set_response(result, response);
    return WS_HANDSHAKE_OK;
}

/* ======================================================================
 * Public API
 * ====================================================================== */

ws_handshake_status ws_handshake_process(const char *request, size_t len,
        const ws_handshake_policy *policy, ws_handshake_result *result) {
    memset(result, 0, sizeof (ws_handshake_result));

    size_t scan_len = len < WS_HANDSHAKE_MAX_SIZE ? len : WS_HANDSHAKE_MAX_SIZE;
    const char *end_of_headers = find_end_of_headers(request, scan_len);
    if (end_of_headers == NULL) {
        if (len >= WS_HANDSHAKE_MAX_SIZE) return respond_error(result, WS_HANDSHAKE_BAD_REQUEST);
        return WS_HANDSHAKE_INCOMPLETE;
    }
    size_t headers_end = (size_t) (end_of_headers - request);
    result->consumed = headers_end + 4;

    ws_request req;
    if (!parse_request(request, headers_end, &req)) return respond_error(result, WS_HANDSHAKE_BAD_REQUEST);

    if (req.host.p == NULL || req.host.len == 0 ||
            !list_contains(req.upgrade, "websocket") ||
            !list_contains(req.connection, "upgrade") ||
            !is_valid_key(req.key))
        return respond_error(result, WS_HANDSHAKE_BAD_REQUEST);

    if (!span_ieq(req.version, "13")) return respond_error(result, WS_HANDSHAKE_UPGRADE_REQUIRED);

    const char *allowed_origins = policy ? policy->allowed_origins : NULL;
    const char *allowed_hosts = policy ? policy->allowed_hosts : NULL;
    if (req.origin.p != NULL && !allowlist_allows(allowed_origins, req.origin))
        return respond_error(result, WS_HANDSHAKE_FORBIDDEN);
    if (!allowlist_allows(allowed_hosts, req.host) && !allowlist_allows(allowed_hosts, host_without_port(req.host)))
        return respond_error(result, WS_HANDSHAKE_FORBIDDEN);

    debug(">>> Incoming websocket connection, key:(%.*s)\n", (int) req.key.len, req.key.p);
    return respond_switching(result, req.key, negotiate_protocol(&req));
}

ws_handshake_status ws_input_filter_handshake(int fd, const char *data, size_t len,
        const ws_handshake_policy *policy, ws_handshake_result *result) {
    buffer_item *slot = ws_buffer_find(fd);
    int buffered = slot != NULL && slot->received_len > 0;

    if (buffered) {
        size_t old_len = slot->received_len;
        ws_buffer_resize(slot, old_len, old_len + len);
        memcpy(slot->received + old_len, data, len);
        data = slot->received;
        len = slot->received_len;
    }

    ws_handshake_status status = ws_handshake_process(data, len, policy, result);

    if (status == WS_HANDSHAKE_INCOMPLETE) {
        if (!buffered) {
            slot = ws_buffer_get_or_add(fd);
            if (slot == NULL) return respond_error(result, WS_HANDSHAKE_BAD_REQUEST); // out of slots
            ws_buffer_resize(slot, 0, len);
            memcpy(slot->received, data, len);
        }
        return status;
    }

    // Done: keep only the bytes after the request (early WebSocket frames).
    size_t left = status == WS_HANDSHAKE_OK ? len - result->consumed : 0;
    if (buffered) {
        if (left > 0) memmove(slot->received, slot->received + result->consumed, left);
        ws_buffer_resize(slot, slot->received_len, left);
    } else if (left > 0) {
        slot = ws_buffer_get_or_add(fd);
        if (slot != NULL) {
            ws_buffer_resize(slot, 0, left);
            memcpy(slot->received, data + result->consumed, left);
        }
    }
    slot = ws_buffer_find(fd);
    if (slot != NULL && ws_buffer_is_idle(slot)) ws_buffer_free(slot);
    return status;
}

ws_conn_state ws_conn_route(int fd, const char *data, size_t len) {
    ws_conn_state state = ws_conn_get_state(fd);
    if (state == CONN_NEW && len > 0) {
        state = ws_is_http_request_start(data, len) >= 0 ? CONN_HTTP_PENDING : CONN_RAW_STOMP;
        ws_conn_set_state(fd, state);
    }
    return state;
}

void ws_handshake_result_free(ws_handshake_result *result) {
    free(result->response);
    memset(result, 0, sizeof (ws_handshake_result));
}
