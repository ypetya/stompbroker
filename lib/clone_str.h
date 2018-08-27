/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   clone_str.h
 * Author: peter
 *
 * Created on July 13, 2018, 11:45 AM
 */

#ifndef CLONE_STR_H
#define CLONE_STR_H

#ifdef __cplusplus
extern "C" {
#endif


char* clone_str(char* src);
char* clone_str_len(char* src, size_t len);


#ifdef __cplusplus
}
#endif

#endif /* CLONE_STR_H */

