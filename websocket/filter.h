

/* 
 * File:   filter.h
 * Author: peter
 *
 * Created on December 20, 2018, 2:55 PM
 * 
 * Websocket
 * 
 */

#ifndef FILTER_H
#define FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../server/data/string_message.h"
#include "../lib/thread_safe_queue.h"

    enum ws_filter_auth_status {
        WS_NO_NEED_OF_HANDSHAKE,
        WS_NEED_OF_HANDSHAKE
    };

    /**
     * Responsibilities:
     *  
     * 1. Respond with websocket hand-shake request on http_requests
     * 2. decode websocket data frames and mark session to encoded
     * 
     * @param out output message queue for sending response
     * @param m incoming message buffer from file descriptor
     * @return 
     */
    int ws_input_filter(ts_queue *out, message * m);
    
    /**
     * In case of an encoded session is present, encodes data into ws frames data
     * 
     * @param m
     */
    void ws_output_filter(message *m);



#ifdef __cplusplus
}
#endif

#endif /* FILTER_H */

