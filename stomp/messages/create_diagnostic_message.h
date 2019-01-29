#ifndef CREATE_DIAGNOSTIC_MESSAGE_H
#define CREATE_DIAGNOSTIC_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../server/data/message/with_timestamp.h"
#include "../../server/data/message/with_payload_length.h"
#include "../parser.h"
#include "../../lib/queue.h"
#include "../../lib/thread_safe_queue.h"
#include "../../lib/associative_array.h"

message_with_frame_len * create_diagnostic_message(
    message_with_timestamp *input,
    parsed_message * pm,
    ts_queue * q_in,
    ts_queue * q_out
    );

void create_diagnostic_headers(associative_array * headers, char* message_body,
    ts_queue * q_in,
    ts_queue * q_out,
    queue * q_stale);

#ifdef __cplusplus
}
#endif

#endif /* CREATE_DIAGNOSTIC_MESSAGE_H */

