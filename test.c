#include <stdio.h>
#include <stdlib.h>
#include "lib/minunit.h"

#include "print_system_info_test.c"

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
