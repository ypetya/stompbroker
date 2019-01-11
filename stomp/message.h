/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   message.h
 * Author: peter
 *
 * Created on July 13, 2018, 3:45 PM
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lib/associative_array.h"
#include "../server/data/string_message.h"

    message_with_frame_len * message_error(int fd, char *reason);
    message_with_frame_len * message_connected(int fd, int session_id);
    message_with_frame_len * message_disconnect(int fd);
    message_with_frame_len * message_receipt(int fd, char* receipt_id);
    message_with_frame_len * message_send(int fd, int subscription_id,
            int message_id, char* dest, char* body);
    message_with_frame_len * message_send_with_headers(int fd, associative_array * headers,
            char* body);
    message * message_poison_pill();
    message_with_frame_len * message_wl_poison_pill();
    
    message_with_frame_len * message_diagnostic(int fd, char * key, char * value);

#ifdef __cplusplus
}
#endif

#endif /* MESSAGE_H */

