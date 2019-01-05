/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   handshake.h
 * Author: peter
 *
 * Created on December 30, 2018, 9:14 PM
 */

#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#ifdef __cplusplus
extern "C" {
#endif


#include "../../server/data/string_message.h"
#include "../../lib/thread_safe_queue.h"

    typedef enum ws_filter_auth_status_t {
        WS_NO_NEED_OF_HANDSHAKE,
        WS_NEED_OF_HANDSHAKE
    } ws_filter_auth_status;
    /**
     * Responsibilities:
     *  
     * 1. Respond with websocket hand-shake request on http_requests
     * 2. decode websocket data frames and mark session to encoded
     * 
     * @param out output message queue for sending response
     * @param m incoming message buffer from file descriptor
     * @return ws_filter_auth_status
     */
    ws_filter_auth_status ws_input_filter_handshake(ts_queue *out, message * m);



#ifdef __cplusplus
}
#endif

#endif /* HANDSHAKE_H */

