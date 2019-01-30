
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

    inline static void * emalloc(size_t size);
    inline static void *erealloc(void *ptr, size_t size);


#ifdef __cplusplus
}
#endif

#endif /* EMALLOC_H */

