
#include "filter.h"
#include <string.h>
#include "../logger.h"

char* is_http_request(char * buffer);

int ws_filter_auth(ts_queue *out, message * m) {

    if (is_http_request(m->content)) {
        debug("%s\n", m->content);
        return WS_NEED_OF_HANDSHAKE;
    }

    return WS_NO_NEED_OF_HANDSHAKE;
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