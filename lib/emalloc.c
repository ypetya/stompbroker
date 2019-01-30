
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emalloc.h"

inline static void *emalloc(size_t size) {
    void *p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    memset(p, '\0', size);

    return p;
}

inline static void *erealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }

    return p;
}