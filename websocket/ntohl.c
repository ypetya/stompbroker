
#include <inttypes.h>
#include <netinet/in.h>

/** 
 * Converts the unsigned 64 bit integer from host byte order to network byte 
 * order.
 */
uint64_t ntohl64(uint64_t value) {
    static const int num = 42;

    /**
     * If these check is true, the system is using the little endian 
     * convention. Else the system is using the big endian convention, which
     * means that we do not have to represent our integers in another way.
     */
    if (*(char *) &num == 42) {
        const uint32_t high = (uint32_t) (value >> 32);
        const uint32_t low = (uint32_t) (value & 0xFFFFFFFF);

        return (((uint64_t) (htonl(low))) << 32) | htonl(high);
    } else {
        return value;
    }
}