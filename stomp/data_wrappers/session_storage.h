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
    
    typedef struct session_st {
        int external_id;
        int session_id;
    } session_item;

    void session_storage_init();
#include "../../lib/thread_safe_queue.h"
    void session_storage_dispose(ts_queue* output_q);
    
    /** 
     @return return index if inserted, -1 if already exists.*/
    int session_storage_add_new(int external_id);
    int session_storage_fetch_client_id(int external_id);
    int session_storage_fetch_external_id(int client_id);
    void session_storage_remove(int index);

    int session_storage_size();

#ifdef __cplusplus
}
#endif

#endif /* SESSION_H */

