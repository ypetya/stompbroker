
#include "filter.h"
#include <string.h>
#include <inttypes.h>
#include "../logger.h"
#include "../lib/thread_safe_queue.h"
#include "../server/data/string_message.h"
#include "../server/data/session_storage.h"
#include "../parse_args.h"

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
char* decode_websocket_frame(char * buffer);
int encode_websocket_frame(char * buffer, char** out);
char* parse_sec_websocket_key(char * buffer);
char* create_accept_key(char * client_key);
void send_accept_response(char* acceptKey, int fd, ts_queue *out);

int ws_input_filter(ts_queue *out, message * m) {
    if (is_http_request(m->content)) {
        char * client_key = parse_sec_websocket_key(m->content);

        if (client_key != NULL) {
            char * acceptKey = create_accept_key(client_key);
            send_accept_response(acceptKey, m->fd, out);
            free(acceptKey);
        }

        return WS_NEED_OF_HANDSHAKE;
    }
    char* decoded_message = decode_websocket_frame(m->content);
    if (decoded_message) {
        free(m->content);
        m->content = decoded_message;
        session_set_encoded(m->fd);
    }

    return WS_NO_NEED_OF_HANDSHAKE;
}

size_t ws_output_filter(message *m) {
    size_t len = strlen(m->content);
    if (session_is_encoded(m->fd)) {
        char* encoded_message;
        len = encode_websocket_frame(m->content, &encoded_message);
        free(m->content);
        m->content = encoded_message;
    }
    return len;
}

/** 
 * Converts the unsigned 64 bit integer from host byte order to network byte 
 * order.
 */
uint64_t ntohl64(uint64_t value) {
    static const int num = 42;

    /**
     * If these check is true, the system is using the little endian 
     * convention. Else the system is using the big endian convention, which
     * means that we do not have to represent our integers in another way.
     */
    if (*(char *) &num == 42) {
        const uint32_t high = (uint32_t) (value >> 32);
        const uint32_t low = (uint32_t) (value & 0xFFFFFFFF);

        return (((uint64_t) (htonl(low))) << 32) | htonl(high);
    } else {
        return value;
    }
}

int encode_websocket_frame(char * buffer, char** out) {
    int skip, len = strlen(buffer)+1, orig_len = len;
    char * encoded_message = NULL;
    if (len < 126) {
        len += 2;
        encoded_message = emalloc(sizeof (char)*len);
        encoded_message[0] = '\x81'; // FIN + CONT
        encoded_message[1] = len;
        memcpy(encoded_message + 2, buffer, orig_len);
    } else if (len < 65536) {
        len += 4;
        encoded_message = emalloc(sizeof (char)*len);
        encoded_message[0] = '\x81'; // FIN + CONT
        encoded_message[1] = 126;
        uint16_t sz16 = htons(len);
        memcpy(encoded_message + 2, &sz16, sizeof (uint16_t));
        memcpy(encoded_message + 4, buffer, orig_len);
    } else {
        len += 10;
        encoded_message = emalloc(sizeof (char)*len);
        encoded_message[0] = '\x81'; // FIN + CONT
        encoded_message[1] = 127;
        uint64_t sz64 = ntohl64(len);
        memcpy(encoded_message + 2, &sz64, sizeof (uint64_t));
        memcpy(encoded_message + 10, buffer, orig_len);
    }
    *out = encoded_message;
    return len;
}

/**
 * Decode websocket data-frame if mask bit is set
 * 
 * Not supporting fragmented frames.
 * Not supporting multiple frames in buffer.
 * Not supporting larger then cfg->input_buffer_size messages.
 * Opcode is ignored.
 * 
 * @param buffer
 * @return 
 */
char* decode_websocket_frame(char * buffer) {
    //int rsv123 = buffer[0] & 0xE;
    int has_mask = has_mask = buffer[1] & 0x80 ? 1 : 0;
    // Websocket client must send a mask! we use it for frame detection
    if (has_mask) {
        int fin = (buffer[0] & 0x80 ? 1 : 0);
        int op_code = (buffer[0] & 0xF);

        uint64_t payload_len = 0;

        int len = buffer[1] & 0x7f;
        int skip = 0;
        char mask[4];
        if (len < 126) {
            payload_len += len;
            memcpy(&mask, buffer + 2, sizeof (mask));
            skip = 6;
        } else if (len == 126) {
            uint16_t sz16;
            memcpy(&sz16, buffer + 2, sizeof (uint16_t));
            payload_len += ntohs(sz16);
            memcpy(&mask, buffer + 4, sizeof (mask));
            skip = 8;
        } else if (len == 127) {
            uint64_t sz64;
            memcpy(&sz64, buffer + 2, sizeof (uint64_t));
            payload_len = ntohl64(sz64);
            memcpy(&mask, buffer + 10, sizeof (mask));
            skip = 14;
        }
        debug(">>> Websocket data frame FIN: %d opcode: 0x%x payload_len: %" PRIu64 "\n", fin, op_code, payload_len);

        stomp_app_config * cfg = config_get_config();
        if (payload_len < cfg->input_buffer_size) {
            char * decoded_message = emalloc(sizeof (char) * (payload_len + 1));

            memcpy(decoded_message, buffer + skip, payload_len);
            for (int i = 0; i < payload_len; i++) decoded_message[i] = decoded_message[i] ^ mask[i % 4];

            return decoded_message;
        } else {
            char * invalid = "INVALID\n\nMessage too large\n";

            return clone_str(invalid);
        }
    }
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
        debug(">>> Incoming websocket connection, parsed key:(%s)\n", key)
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
