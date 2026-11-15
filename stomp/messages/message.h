#ifndef MESSAGE_H
#define MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lib/associative_array.h"
#include "../../server/data/message/with_payload_length.h"
#include "../../server/data/message/with_timestamp.h"

    /** ERROR frame (with receipt-id when not NULL); the connection is closed after it. */
    message_with_frame_len * message_error(int fd, const char *reason, const char *receipt_id);
    /** ERROR frame for a failed version negotiation, with a version header. */
    message_with_frame_len * message_error_versions(int fd, const char *reason, const char *receipt_id,
            const char *supported_versions);
    message_with_frame_len * message_connected(int fd, int session_id, const char *version);
    /** Nothing to send, only close the connection after the messages before it. */
    message_with_frame_len * message_close_request(int fd);
    message_with_frame_len * message_disconnect(int fd);
    message_with_frame_len * message_receipt(int fd, char* receipt_id);
    message_with_frame_len * message_send(int fd, int subscription_id,
            int message_id, char* dest, char* body);
    message_with_frame_len * message_send_with_headers(int fd, associative_array * headers,
            const char* body, size_t body_len);
    message_with_frame_len * message_diagnostic(int fd, char * key, char * value);

#ifdef __cplusplus
}
#endif

#endif /* MESSAGE_H */

