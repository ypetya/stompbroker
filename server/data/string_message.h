

/* 
 * File:   message.h
 * Author: peter
 *
 * Created on December 20, 2018, 3:04 PM
 */

#ifndef STRING_MESSAGE_H
#define STRING_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct msg_st {
        int fd;
        char * content;
    } message;
    
    typedef struct msg_wl_st {
        int fd;
        char * content;
        size_t frame_len;
    } message_with_frame_len;

    message * message_create(int fd, char * str);
    message_with_frame_len * message_create_with_frame_len(int fd, char * str);
    
    void message_destroy(message * m);
#ifdef __cplusplus
}
#endif

#endif /* STRING_MESSAGE_H */

