
#ifndef PARSER_H
#define PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#define FRM_CONNECT "CONNECT"
#define FRM_CONNECT_LEN 7
#define FRM_CONNECT_ID 1
#define FRM_STOMP "STOMP"
#define FRM_STOMP_LEN 5
#define FRM_DISCONNECT "DISCONNECT"
#define FRM_DISCONNECT_LEN 10
#define FRM_DISCONNECT_ID 2
#define FRM_SUBSCRIBE "SUBSCRIBE"
#define FRM_SUBSCRIBE_LEN 9
#define FRM_SUBSCRIBE_ID 3
#define FRM_UNSUBSCRIBE "UNSUBSCRIBE"
#define FRM_UNSUBSCRIBE_LEN 11
#define FRM_UNSUBSCRIBE_ID 4
#define FRM_SEND "SEND"
#define FRM_SEND_LEN 4
#define FRM_SEND_ID 5
#define FRM_DIAGNOSTIC "DIAG"
#define FRM_DIAGNOSTIC_LEN 4
#define FRM_DIAGNOSTIC_ID 6

#include "../server/data/message/with_timestamp.h"
#include "../lib/associative_array.h"
#include <time.h>

    typedef struct parsed_message_st {
        int command;
        associative_array *headers;
        char* receipt_id;
        char* message_body;
        // SUBSCRIPTION destination
        char* topic;
        // SUBSCRIPTION id
        char* id;
        // time-stamp
        clock_t ts;
    } parsed_message;

    parsed_message* parse_message(message_with_timestamp* message);

    void free_parsed_message(parsed_message* pm);

#ifdef __cplusplus
}
#endif

#endif /* PARSER_H */

