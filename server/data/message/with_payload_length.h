
#ifndef STRING_MESSAGE_WITH_PAYLOAD_LENGTH_H
#define STRING_MESSAGE_WITH_PAYLOAD_LENGTH_H

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef struct message_with_length_t {
        int fd;
        char * content;
        size_t frame_len;
    } message_with_frame_len;

    message_with_frame_len * message_create_with_frame_len(int fd, char * str);
    
    void message_destroy_with_frame_len(message_with_frame_len *m);

#ifdef __cplusplus
}
#endif

#endif /* STRING_MESSAGE_WITH_PAYLOAD_LENGTH_H */

