

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

#include "../lib/string_message.h"
#include "../lib/thread_safe_queue.h"

    enum ws_filter_auth_status {
        WS_NO_NEED_OF_HANDSHAKE,
        WS_NEED_OF_HANDSHAKE
    };

    int ws_filter_auth(ts_queue *out, message * m);



#ifdef __cplusplus
}
#endif

#endif /* FILTER_H */

