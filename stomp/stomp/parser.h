
#ifndef PARSER_H
#define PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#define FRM_CONNECT "CONNECT"
#define FRM_CONNECT_ID 1
#define FRM_STOMP "STOMP"

#include "message.h"
#include "../../lib/associative_array.h"

    typedef struct parsed_message_st {
        int command;
        associative_array *headers;

        char* message_body;
    } parsed_message;

    parsed_message* parse_message(message* message);
    
    void free_parsed_message(parsed_message* pm); 

#ifdef __cplusplus
}
#endif

#endif /* PARSER_H */

