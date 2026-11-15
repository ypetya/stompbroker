#ifndef WS_NTOHL_H
#define WS_NTOHL_H

#include <stdint.h>

/** Converts a 64 bit integer between host and network byte order (symmetric). */
uint64_t ntohl64(uint64_t value);

#endif /* WS_NTOHL_H */
