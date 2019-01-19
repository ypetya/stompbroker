#ifndef CREATE_DIAGNOSTIC_MESSAGE_H
#define CREATE_DIAGNOSTIC_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../server/data/message/with_timestamp.h"
#include "../parser.h"

message_with_frame_len * create_diagnostic_message(message_with_timestamp *input, parsed_message * pm);

#ifdef __cplusplus
}
#endif

#endif /* CREATE_DIAGNOSTIC_MESSAGE_H */

