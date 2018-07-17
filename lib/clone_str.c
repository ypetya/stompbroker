#include <string.h>

#include "emalloc.h"
#include "clone_str.h"

char* clone_str(char* src) {
    int len = strlen(src);
    char* text = (char*)emalloc(len + 1);
    strcpy(text, src);
    // in emalloc
    //text[len] = '\0';
    return text;
}

char* clone_str_len(char* src, size_t len) {
    char* text = (char*)emalloc(len + 1);
    strncpy(text, src, len);
    // in emalloc
    //text[len] = '\0';
    return text;
}