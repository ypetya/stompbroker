/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   cleanup.h
 * Author: peter
 *
 * Created on December 30, 2018, 9:16 PM
 */

#ifndef CLEANUP_H
#define CLEANUP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lib/thread_safe_queue.h"

/**
 * Closes a connection. The epoll thread is the only caller: writers request a
 * close with shutdown(), which makes the epoll thread read EOF and call this.
 *
 * Steps: shut the socket down (a writer blocked on the peer gives up), wait
 * until no writer holds a batch of the fd, queue the purge of the STOMP
 * session for the reader, end the session (its generation increases, so
 * output still queued for it is dropped), close the fd and reset all per-fd
 * state: slow consumer budget, WebSocket buffers and STOMP input buffer.
 */
void connection_close(int fd, ts_queue * input_queue, ts_queue * output_queue);


#ifdef __cplusplus
}
#endif

#endif /* CLEANUP_H */

