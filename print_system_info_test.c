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
