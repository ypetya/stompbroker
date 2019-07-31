
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emalloc.h"

inline static void *emalloc(size_t size) {
    void *p = calloc(size,1);
    if (!p) {
        perror("Could not allocate memory!");
        exit(EXIT_FAILURE);
    }

    return p;
}

inline static void *erealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p) {
        perror("Could not reallocate memory!");
        exit(EXIT_FAILURE);
    }

    return p;
}