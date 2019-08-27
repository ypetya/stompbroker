#include "lib/minunit.h"

#include <stdio.h>
int calls=0;

typedef char STR[100];

STR strings[2] = {0};

// mock info
#define SERVER_LOGGER_H
#define info(...) sprintf(strings[calls++], __VA_ARGS__)

#include "print_system_info.c"

#include <string.h>
static char * test_print_system_info() {
    print_system_info();
    
    int str1 = strcmp("system: Has 4 processors configured and 4 processors available.\n", strings[0]);
    //printf(strings[0]);
    mu_assert("First string does not match!", str1==0 );
    
    
    str1 = strcmp("system: Maximum file descriptors (ulimit): 1048576.\n", strings[1]);
    //printf(strings[1]);
    mu_assert("Second string does not match!", str1==0 );
    
    
    return 0;
}


 static char * all_tests() {
     mu_run_test(test_print_system_info);
     return 0;
 }
 
 int main(int argc, char **argv) {
     char *result = all_tests();
     if (result != 0) {
         printf("%s\n", result);
     }
     else {
         printf("ALL TESTS PASSED\n");
     }
     printf("Tests run: %d\n", tests_run);
 
     return result != 0;
 }
