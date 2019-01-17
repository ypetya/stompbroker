
#include <stdint.h>
#include "session.h"

// bitmask of connected session_id-s
uint64_t session_connected[STOMP_SESSION_SIZE];

void stomp_session_init() {
    for (int i = 0; i < STOMP_SESSION_SIZE; i++) {
        session_connected[i] = 0U;
    }
}

int stomp_session_is_connected(int fd) {
    int i, c, n;
    // find the bit
    for (i = 0, c = fd; c > 0; i++, c = c / 64, n = c % 64);

    uint64_t mask = 1U << n;

    return session_connected[i] & mask;
}

void stomp_session_set_connected(int fd, int connected) {
    int i, c, n;
    // find the bit
    for (i = 0, c = fd; c > 0; i++, c = c / 64, n = c % 64);

    uint64_t mask = 1U << n;
    if (connected == 0) mask ^= UINT64_MAX;

    // erase or set the bit
    session_connected[i] = connected == 0 ? session_connected[i] & mask : session_connected[i] | mask;
}

int stomp_session_connected_size() {
    //count the bits
    int n = 0;
    uint64_t one = 1;
    for (int i = 0; i < STOMP_SESSION_SIZE; i++) {
        uint64_t b = session_connected[i];
        if (one & b) n++;
        for (int j = 1; j < 63; j++) {
            if ((one << j) & b) {
                n++;
            }
        }
    }
    return n;
}
