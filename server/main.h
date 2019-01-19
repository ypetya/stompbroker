
#ifndef SERVER_MAIN_H
#define SERVER_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "data/message/with_timestamp.c"
#include "data/message/with_payload_length.c"
#include "data/cleanup.c"
#include "data/session_storage_simple.c"
#include "signal_action_handler.c"
#include "listen.c"
#include "writer_thread.c"
#include "reader_thread.c"
#include "process.c"
#include "accept_epoll.c"

#ifdef __cplusplus
}
#endif

#endif 

