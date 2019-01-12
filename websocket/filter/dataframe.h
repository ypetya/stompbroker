/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   dataframe.h
 * Author: peter
 *
 * Created on December 30, 2018, 9:15 PM
 */

#ifndef DATAFRAME_H
#define DATAFRAME_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum ws_filter_dataframe_status_t {
        WS_NOT_A_DATAFRAME = 0,
        WS_COMPLETE_DATAFRAME = 1, // out parameter contains at least one full decoded frame
        WS_INCOMPLETE_DATAFRAME = 2, // out parameter does not contain decoded data
        WS_TOO_LARGE_DATAFRAME = -1,
        WS_BUFFER_EXCEEDED_MAX = -2,
        WS_INVALID_HEADER = -3
    } ws_filter_dataframe_status;


    ws_filter_dataframe_status ws_input_filter_dataframe(int fd, char* buffer, size_t read_len, char** out, size_t *decoded_buf_len);


#include "../../server/data/string_message.h"
    /**
     * In case of an encoded session is present, encodes data into ws frames data
     * 
     * @param m
     * @return message->content size
     */
    size_t ws_output_filter(message_with_frame_len *m);



#ifdef __cplusplus
}
#endif

#endif /* DATAFRAME_H */

