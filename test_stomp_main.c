/* Standalone runner for the portable STOMP unit tests (see Makefile: test-stomp). */
#include "test_stomp.c"

int main(void) {
    return run_stomp_tests() == 0 ? 0 : 1;
}
