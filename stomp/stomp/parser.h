
#ifndef PARSER_H
#define PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#define FRM_CONNECT "CONNECT"
#define FRM_CONNECT_ID 1
#define FRM_STOMP "STOMP"
#define FRM_DISCONNECT "DISCONNECT"
#define FRM_DISCONNECT_ID 2
#define FRM_SUBSCRIBE "SUBSCRIBE"
#define FRM_SUBSCRIBE_ID 3
#define FRM_UNSUBSCRIBE "UNSUBSCRIBE"
#define FRM_UNSUBSCRIBE_ID 4
#define FRM_SEND "SEND"
#define FRM_SEND_ID 5
    
#include "message.h"
#include "../../lib/associative_array.h"

    typedef struct parsed_message_st {
        int command;
        associative_array *headers;
        char* receipt_id;
        char* message_body;
        // SUBSCRIPTION destination
        char* topic;
        // SUBSCRIPTION id
        int id;
    } parsed_message;

    parsed_message* parse_message(message* message);
    
    void free_parsed_message(parsed_message* pm); 

#ifdef __cplusplus
}
#endif

#endif /* PARSER_H */

