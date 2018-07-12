/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   accept.h
 * Author: peter
 *
 * Created on July 12, 2018, 5:43 PM
 */

#ifndef ACCEPT_H
#define ACCEPT_H

#ifdef __cplusplus
extern "C" {
#endif

void acceptIncomingDataLoop(int listenSockFD);

#ifdef __cplusplus
}
#endif

#endif /* ACCEPT_H */

