/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   message.h
 * Author: peter
 *
 * Created on July 13, 2018, 3:45 PM
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msg_st {
    int fd;
    char * content;
} message;

message * message_create(int fd, char * str, int len);
void message_destroy(message * m);

#ifdef __cplusplus
}
#endif

#endif /* MESSAGE_H */

