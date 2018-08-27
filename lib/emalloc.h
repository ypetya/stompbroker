/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   emalloc.h
 * Author: peter
 *
 * Created on July 11, 2018, 3:06 PM
 */

#ifndef EMALLOC_H
#define EMALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

    static void *emalloc(size_t size);
    static void *erealloc(void *ptr, size_t size);


#ifdef __cplusplus
}
#endif

#endif /* EMALLOC_H */

