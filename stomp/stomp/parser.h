
#ifndef PARSER_H
#define PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_CONNECT "CONNECT"
#define FRAME_STOMP "STOMP"

#include "message.h"
#include "../../lib/associative_array.h"
    typedef struct parsed_message_st {
        int command;
        associative_array headers;
        
        char* message_body;
    } parsed_message;
    
    parsed_message* parseMessage(message* message);

#ifdef __cplusplus
}
#endif

#endif /* PARSER_H */

