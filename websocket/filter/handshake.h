/*
 * File:   handshake.h
 * Author: peter
 *
 * Created on December 30, 2018, 9:14 PM
 *
 * RFC 6455 opening handshake (server side).
 */

#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include <stddef.h>
#include "../buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

    /** Upper bound of an upgrade request (request line + headers). */
#define WS_HANDSHAKE_MAX_SIZE 8192

    typedef enum ws_handshake_status_t {
        WS_HANDSHAKE_INCOMPLETE = 0, // request not complete yet, keep buffering
        WS_HANDSHAKE_OK = 101, // response is "101 Switching Protocols"
        WS_HANDSHAKE_BAD_REQUEST = 400, // malformed / not a WebSocket upgrade
        WS_HANDSHAKE_FORBIDDEN = 403, // Origin or Host not allowed by the policy
        WS_HANDSHAKE_UPGRADE_REQUIRED = 426 // unsupported Sec-WebSocket-Version
    } ws_handshake_status;

    /**
     * Optional allowlists, comma separated, case-insensitive. NULL or ""
     * allows everything (default). A request without an Origin header (i.e.
     * a non-browser client) is not subject to the Origin allowlist.
     * Host entries match either the full Host value or its host part.
     */
    typedef struct ws_handshake_policy_t {
        const char *allowed_origins;
        const char *allowed_hosts;
    } ws_handshake_policy;

    typedef struct ws_handshake_result_t {
        size_t consumed; // request bytes including the final CRLF CRLF
        char *response; // HTTP response to send (101 or error), heap allocated
        size_t response_len;
        char protocol[16]; // negotiated STOMP subprotocol, "" if none
    } ws_handshake_result;

    /**
     * Is data the start of an HTTP GET request line?
     * @return 1 yes, 0 too short to tell, -1 no
     */
    int ws_is_http_request_start(const char *data, size_t len);

    /**
     * Validates an upgrade request and builds the response. Pure function:
     * request may be partial (returns WS_HANDSHAKE_INCOMPLETE).
     */
    ws_handshake_status ws_handshake_process(const char *request, size_t len,
            const ws_handshake_policy *policy, ws_handshake_result *result);

    /**
     * Connection level: buffers the bytes of fd until the request is complete,
     * then validates it. On completion the request bytes are dropped from the
     * connection buffer; bytes after the request stay buffered as WebSocket
     * input.
     */
    ws_handshake_status ws_input_filter_handshake(int fd, const char *data, size_t len,
            const ws_handshake_policy *policy, ws_handshake_result *result);

    void ws_handshake_result_free(ws_handshake_result *result);

    /**
     * Transport routing for bytes received on fd. A new connection whose first
     * bytes start an HTTP GET request line becomes CONN_HTTP_PENDING, anything
     * else CONN_RAW_STOMP (no STOMP command starts with 'G'). Established
     * states are returned unchanged, so a STOMP payload containing "GET /" is
     * never mistaken for a handshake.
     */
    ws_conn_state ws_conn_route(int fd, const char *data, size_t len);

    /** Sec-WebSocket-Accept value for a client key (heap allocated, NULL on error). */
    char* create_accept_key(const char * client_key);

#ifdef __cplusplus
}
#endif

#endif /* HANDSHAKE_H */
