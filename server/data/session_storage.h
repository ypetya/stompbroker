/* 
 * File:   session.h
 * Author: Peter Kiss <ypetya@gmail.com>
 *
 * Created on July 19, 2018, 4:41 PM
 * 
 * Session storage goals
 * 
 * - to be fast enough: we store file-descriptors directly
 * - add some extra bit flags to the upper bits, out of range
 * - some special flags (isEncoded) can travel to the stomp layer and to the write
 *  layer. This makes checking of the flag to threadsafe.
 * - cmd_purge flag is processed in the stomp layer and use for session disposal
 * - Support diagnostics
 */

#ifndef SESSION_H
#define SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

    // FD space should be 17bits, max-connections: 131072
#define MAX_NUMBER_OF_CONNECTIONS 0x20000
    // the lower 18 bit must contain the valid FD. This covers MAX_NUMBER_OF_CONNECTIONS + 1 bit
#define FD_MASK 0x3FFF
    // the 19th bit describes the encoded status flag
    // this means the session needs to be care as encoded communication channel
#define FD_IS_ENCODED_MASK 0x80000

    // the 20th bit describes the pruge session command flag
    // this will cause STOMP logic to remove subscriptions of the FD and
    // this info can travel with the message
#define FD_CMD_PURGE_SESSION 0x100000

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
    int session_storage_add_new(int fd);
    void session_storage_remove(int fd);


    // flags and manipulation
    /** 
     * Checks the session_storage array 
     * @return >0 if encoded
     */
    int session_storage_is_encoded(int fd);

    /**
     * Does not modify session_storage
     * @param fd
     * @return fd with encoded flag
     */
    int session_set_encoded(int fd);
    int session_without_flags(int fd);
    /** sets the session_storage array as well
     * @return fd, containing the encoded flag
     */
    void session_storage_set_encoded(int fd);

    /**
     * This flag can travel with the message fd
     * @param fd
     * @return 
     */
    int session_set_cmd_purge(int fd);
    int session_is_cmd_purge(int fd);
    int session_wo_cmd_purge(int fd);

    /** checks the fd only 
     * @return >0 if encoded
     */
    int session_is_encoded(int fd);

    /*
     * Only for diagnostic purposes!
     */
    int session_storage_encoded_size();
    int session_storage_size();



#ifdef __cplusplus
}
#endif

#endif /* SESSION_H */
