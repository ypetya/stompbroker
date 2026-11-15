#include "output_batch.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef MSG_NOSIGNAL
#define OUTPUT_SEND_FLAGS MSG_NOSIGNAL
#else
#define OUTPUT_SEND_FLAGS 0 // SIGPIPE is ignored process wide (signal_action_handler.c)
#endif

static long long monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int write_fully(int fd, const char *bytes, size_t len, int timeout_ms) {
    size_t offset = 0;
    long long deadline = monotonic_ms() + timeout_ms;

    while (offset < len) {
        ssize_t written = send(fd, bytes + offset, len - offset, OUTPUT_SEND_FLAGS);
        if (written > 0) {
            offset += (size_t) written;
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            long long remaining = deadline - monotonic_ms();
            if (remaining <= 0) return -1; // slow consumer: give up
            struct pollfd pfd = {.fd = fd, .events = POLLOUT, .revents = 0};
            int ready = poll(&pfd, 1, (int) remaining);
            if (ready < 0 && errno != EINTR) return -1;
            if (ready > 0 && (pfd.revents & (POLLERR | POLLNVAL))) return -1;
            continue;
        }
        return -1; // peer gone or other error
    }
    return 0;
}

int output_batch_flush(output_batch *batch) {
    if (batch->used == 0) return 0;
    int res = write_fully(batch->fd, batch->buffer, batch->used, batch->timeout_ms);
    batch->used = 0;
    return res;
}

int output_batch_add(output_batch *batch, const char *bytes, size_t len) {
    if (batch->used + len > batch->capacity && output_batch_flush(batch) < 0) return -1;
    if (len > batch->capacity) return write_fully(batch->fd, bytes, len, batch->timeout_ms);
    memcpy(batch->buffer + batch->used, bytes, len);
    batch->used += len;
    return 0;
}
