/* 
 * File:   session.h
 * Author: Peter Kiss <ypetya@gmail.com>
 *
 * Created on July 19, 2018, 4:41 PM
 * 
 * Session storage goals
 * 
 * - to be fast enough, client_id <-> external_id in constant time
 * - to store some extra flags about encoding / decoding - websocket
 * - Support create, read, reset and diagnostics
 */

#ifndef SESSION_H
#define SESSION_H

#ifdef __cplusplus
extern "C"
{
#endif

    // FD space should be 17bits, max-connections: 131072
    #define MAX_NUMBER_OF_CONNECTIONS 0x20000
    // the lower 18 bit must contain the valid FD. This is smaller than MAX_NUMBER_OF_CONNECTIONS + some
    #define FD_MASK 0x700F
    // the 19th bit describes the encoded status flag
    // this means the session needs to be care as encoded communication channel
    #define FD_IS_ENCODED_MASK 0x80000
    // the 20th bit describes the connected status flag
    // this means the session needs to be care as connected
    #define FD_IS_CONNECTED_MASK 0x100000

    enum session_status {
        EMPTY_SESSION = 0,
        MAX_SESSION_NUMBER_EXCEEDED = -2,
        SESSION_IS_IN_USE = -1,
        SESSION_IS_INVALID = -1
    };

    void session_storage_init();
    void session_storage_dispose();

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
    void session_storage_remove_external(int external_id);

    /**
     * Only for diagnostic purposes!
     * @return 
     */
    int session_storage_connected_size();
    
    // flags and manipulation
    int session_is_encoded(int external_id);
    void session_set_encoded(int external_id);
    int session_is_connected(int client_id);
    void session_set_connected(int client_id);

#ifdef __cplusplus
}
#endif

#endif /* SESSION_H */
