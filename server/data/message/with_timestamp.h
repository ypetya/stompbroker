#ifndef MESSAGE_WITH_TIMESTAMP_H
#define MESSAGE_WITH_TIMESTAMP_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Messages with timestamp are used in "input queue".
     * Timestamp is used by the TTL logic of the message broker.
    */
    typedef struct msg_with_timestamp_st {
        int fd;
        char * content;
        clock_t ts;
    } message_with_timestamp;

    message_with_timestamp * message_create_with_timestamp(int fd, char * str, clock_t ts);
    
    void message_destroy_with_timestamp(message_with_timestamp * m);

    message_with_timestamp * message_poison_pill_with_timestamp();
#ifdef __cplusplus
}
#endif

#endif /* MESSAGE_WITH_TIMESTAMP_H */

