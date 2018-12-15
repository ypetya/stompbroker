/* 
 * File:   session.h
 * Author: Peter Kiss <ypetya@gmail.com>
 *
 * Created on July 19, 2018, 4:41 PM
 */

#ifndef SESSION_H
#define SESSION_H

#ifdef __cplusplus
extern "C"
{
#endif

    #define MAX_NUMBER_OF_CONNECTIONS 100000

    typedef struct session_st
    {
        int external_id;
        int session_id;
    } session_item;

    void session_storage_init();
#include "../../lib/thread_safe_queue.h"
    void session_storage_dispose(ts_queue *output_q);

    /** 
     * @return return index if inserted, -1 if already exists, -2 if no more conn limit reached
     */
    int session_storage_add_new(int external_id);
    int session_storage_fetch_client_id(int external_id);
    /** 
     * @return external_id if exist, -1 otherwise
     */
    int session_storage_fetch_external_id(int client_id);
    void session_storage_remove(int index);

    int session_storage_size();

#ifdef __cplusplus
}
#endif

#endif /* SESSION_H */
