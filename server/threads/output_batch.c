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

/*
 * Waiting for a slow peer is charged to its per-connection budget: at most
 * timeout_ms of waiting in total, across writes and batches, until the peer
 * has accepted min_bytes (then the budget starts over). The deadline is not
 * moved by progress, so a peer that reads only a trickle cannot keep a writer
 * busy forever by letting each small write finish just before its own
 * deadline. Effective minimum rate: min_bytes per timeout_ms of waiting.
 */
int write_fully_budget(int fd, const char *bytes, size_t len, int timeout_ms, size_t min_bytes, output_stall *stall) {
    size_t offset = 0;

    while (offset < len) {
        ssize_t written = send(fd, bytes + offset, len - offset, OUTPUT_SEND_FLAGS);
        if (written > 0) {
            offset += (size_t) written;
            stall->bytes += (size_t) written;
            if (min_bytes > 0 && stall->bytes >= min_bytes) *stall = (output_stall) {0}; // reading fast enough
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            long long remaining = timeout_ms - stall->waited_ms;
            if (remaining <= 0) return -1; // slow consumer: give up
            struct pollfd pfd = {.fd = fd, .events = POLLOUT, .revents = 0};
            long long started = monotonic_ms();
            int ready = poll(&pfd, 1, (int) remaining);
            stall->waited_ms += monotonic_ms() - started;
            if (ready < 0 && errno != EINTR) return -1;
            if (ready > 0 && (pfd.revents & (POLLERR | POLLNVAL))) return -1;
            continue;
        }
        return -1; // peer gone or other error
    }
    return 0;
}

int write_fully(int fd, const char *bytes, size_t len, int timeout_ms) {
    output_stall stall = {0};
    return write_fully_budget(fd, bytes, len, timeout_ms, 0, &stall);
}

static int batch_write(output_batch *batch, const char *bytes, size_t len) {
    output_stall local = {0};
    return write_fully_budget(batch->fd, bytes, len, batch->timeout_ms, batch->min_bytes,
            batch->stall ? batch->stall : &local);
}

int output_batch_flush(output_batch *batch) {
    if (batch->used == 0) return 0;
    int res = batch_write(batch, batch->buffer, batch->used);
    batch->used = 0;
    return res;
}

int output_batch_add(output_batch *batch, const char *bytes, size_t len) {
    if (batch->used + len > batch->capacity && output_batch_flush(batch) < 0) return -1;
    if (len > batch->capacity) return batch_write(batch, bytes, len);
    memcpy(batch->buffer + batch->used, bytes, len);
    batch->used += len;
    return 0;
}
