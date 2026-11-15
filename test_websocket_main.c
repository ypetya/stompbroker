/* Standalone runner for the portable WebSocket unit tests (see Makefile: test-ws). */
#include "lib/emalloc.c"
#include "lib/clone_str.c"
#include "test_websocket.c"

int main(void) {
    return run_websocket_tests() == 0 ? 0 : 1;
}
