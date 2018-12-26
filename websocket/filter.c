
#include "filter.h"
#include <string.h>
#include "../logger.h"
#include "../lib/thread_safe_queue.h"
#include "../server/data/string_message.h"
#include "../server/data/session_storage.h"

#include "./utils/md5.c"
#include "./utils/sha1.c"
#include "./utils/base64.c"

/**
 * We are supporting websocket RFC6455 only!
 * 
 * This auth filter is detecting HTTP GET requests and does the handshake
 * 
 */
char* is_http_request(char * buffer);
char* decode_websocket_frames(char * buffer);
char* encode_websocket_frame(char * buffer);
char* parse_sec_websocket_key(char * buffer);
char* create_accept_key(char * client_key);
void send_accept_response(char* acceptKey, int fd, ts_queue *out);

int ws_input_filter(ts_queue *out, message * m) {
    if (is_http_request(m->content)) {
        debug("%s\n", m->content);

        char * client_key = parse_sec_websocket_key(m->content);

        if (client_key != NULL) {
            char * acceptKey = create_accept_key(client_key);
            send_accept_response(acceptKey, m->fd, out);
            free(acceptKey);
        }

        return WS_NEED_OF_HANDSHAKE;
    }
    char* decoded_message = decode_websocket_frames(m->content);
    if (decoded_message) {
        free(m->content);
        m->content = decoded_message;
        session_set_encoded(m->fd);
    }

    return WS_NO_NEED_OF_HANDSHAKE;
}

void ws_output_filter(message *m) {
    if (session_is_encoded(m->fd)) {
        char* encoded_message = encode_websocket_frame(m->content);
        free(m->content);
        m->content = encoded_message;
    }
}

char* encode_websocket_frame(char * buffer) {
    
}

char* decode_websocket_frames(char * buffer) {
    //TODO implement websocket decoding
    return NULL;
}

char * WEBSOCKET_MAGIC_KEY = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

char* create_accept_key(char * client_key) {
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

#define WEBSOCKET_KEY_HEADER_LEN 19
const char * WEBSOCKET_KEY_HEADER = "Sec-WebSocket-Key: "; // len=19

char* parse_sec_websocket_key(char * buffer) {
    char * match = strstr(buffer, WEBSOCKET_KEY_HEADER);
    if (match != NULL) {
        char * eok = strstr(buffer, "==");
        if (eok != NULL) {
            eok[2] = '\0';
        }
        char * key = match + WEBSOCKET_KEY_HEADER_LEN;
        debug("Parsed key:(%s)\n", key)
        return key;
    }
    return NULL;
}

const char * WEB_REQUEST_1 = "GET /";
//const char * WEB_REQUEST_2 = "HTTP/1.1";

char * is_http_request(char * buffer) {
    char * first_ln = strchr(buffer, '\n');

    if (first_ln != NULL) {
        char * match = strstr(buffer, WEB_REQUEST_1);
        if (match != NULL && match < first_ln) return first_ln + 1;
    }

    return NULL;
}

char * ACCEPT_TEMPLATE = "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n" // We are supporting only RFC6455 !!!
        "Sec-WebSocket-Accept: %s\r\n\r\n";

void send_accept_response(char* acceptKey, int fd, ts_queue *out) {
    int key_length = strlen(acceptKey);
    size_t len = strlen(ACCEPT_TEMPLATE) - 2 + key_length + 1;
    char * frame = emalloc(len);
    sprintf(frame, ACCEPT_TEMPLATE, acceptKey);

    message * msg = message_create(fd, frame);
    free(frame);

    ts_enqueue(out, msg);
}
