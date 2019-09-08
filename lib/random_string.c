#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "./emalloc.h"

//              012345678901234567890123456
char * CHARS = "abcdefghijklmnopqrstuvwzxy ";

void print_random_str(int len) {
    srand(time(NULL));

    for (int i = 0; i < len; i++) {
        int chr = (int)CHARS[rand() % 27];
        putc(chr, stdout);
    }
    
    fflush(stdout);
}

char * random_str(int len) {
    char * ret = emalloc(len+1);
    for (int i = 0; i < len; i++) {
        int chr = (int)CHARS[rand() % 27];
        ret[i]=chr;
    }
    
    return ret;
}