/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   session.h
 * Author: peter
 *
 * Created on January 5, 2019, 11:01 AM
 */

#ifndef STOMP_SESSION_H
#define STOMP_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

    // the size in bits
#define STOMP_SESSION_SIZE (MAX_NUMBER_OF_CONNECTIONS/64)
    /** 
     * must work on bit flags to spare a huge amount of space
     */
    int stomp_session_is_connected(int fd);
    void stomp_session_set_connected(int fd, int is_connected);
    int stomp_session_connected_size();

    void stomp_session_init();

#ifdef __cplusplus
}
#endif

#endif /* STOMP_SESSION_H */

