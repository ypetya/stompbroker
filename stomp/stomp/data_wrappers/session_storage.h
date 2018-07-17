/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   session.h
 * Author: peter
 *
 * Created on July 19, 2018, 4:41 PM
 */

#ifndef SESSION_H
#define SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

    void session_storage_init();
#include "../../../lib/thread_safe_queue.h"
    void session_storage_dispose(ts_queue* output_q);
    
    /** 
     @return return index if inserted, -1 if already exists.*/
    int session_storage_add_new(int client_fd);
    int session_storage_find(int client_fd);
    void session_storage_remove(int client_id);


#ifdef __cplusplus
}
#endif

#endif /* SESSION_H */

