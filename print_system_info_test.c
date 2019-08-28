#include <stdio.h>
#include <string.h>
#include "lib/minunit.h"

#define LINE_LEN 100
typedef char STR[LINE_LEN];

// BEGIN mock logs info
int calls=0;
STR strings[2] = {0};

#define SERVER_LOGGER_H
#define info(...) snprintf(strings[calls++], LINE_LEN, __VA_ARGS__)

#include "print_system_info.c"

#undef SERVER_LOGGER_H
#undef info
// END mock logs info

int is_numeric(char c) {
    return c >= '0' && c <= '9';
}

/**
 * s1 can contain numeric characters but will get skipped at comparison
 * @param s1
 * @param s2
 * @return 0 if equals - omitting the numerals in s1
 */
int strcmp_wo_nums(char* s1,char* s2) {
    if(!s1 || !s2) return -1;
    
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    if(len1<len2) return -1;
    
    for(int i=0,j=0;j<len2;){
        if(is_numeric(s1[i])) {
            i++;
            continue;
        };
        if(s1[i]!=s2[j]) return s1[i]-s2[i];
        i++;
        j++;
    }
    
    return 0;
}

static char * test_print_system_info() {
    print_system_info();
    
    int str1 = strcmp_wo_nums(
        strings[0],
        "system: Has  processors configured and  processors available.\n");
    
    //printf(strings[0]);
    mu_assert("Expected to print available processors!", str1==0 );
    
    
    str1 = strcmp_wo_nums(
            strings[1],
            "system: Maximum file descriptors (ulimit): .\n");
    
    //printf(strings[1]);
    mu_assert("Expected to print maximum file descriptors!", str1==0 );
    
    
    return 0;
}
