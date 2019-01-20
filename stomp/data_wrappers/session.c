
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
    
    // find the bit
    int n = fd % 64;
    int i = fd /64;

    uint64_t mask = 1U << (n%32);
    if(n>=32) mask*= ((uint64_t)UINT32_MAX+1);

    if( (session_connected[i] & mask) != 0U) return 1;
    return 0;
}

void stomp_session_set_connected(int fd, int connected) {
    
    // find the bit
    int n = fd % 64;
    int i = fd /64;
    
    uint64_t mask = 1U << (n%32);
    if(n>=32) mask*= ((uint64_t)UINT32_MAX+1);

    // erase or set the bit
    if (connected == 0){
        mask ^= UINT64_MAX;
        session_connected[i] = session_connected[i] & mask;
    } else {
        session_connected[i] = session_connected[i] | mask;
    }
}

int stomp_session_connected_size() {
    //count the bits
    int n = 0;
    uint64_t one = 1;
    for (int i = 0; i < STOMP_SESSION_SIZE; i++) {
        uint64_t b = session_connected[i];
        uint64_t bu = session_connected[i]/((uint64_t)UINT32_MAX+1);
        uint64_t mask;
        for (int j = 0; j < 32; j++) {
            mask = (one << j);
            if (mask & b) n++;
            if (mask & bu) n++;
        }
    }
    return n;
}
