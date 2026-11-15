#ifndef OUTPUT_BATCH_H
#define OUTPUT_BATCH_H

#include <stddef.h>

/**
 * Batched, bounded socket output for the writer threads.
 *
 * Messages are packed into a fixed size buffer; the buffer is flushed when
 * the next message would not fit, and a message larger than the whole buffer
 * is written directly (never copied past the buffer end). Writes keep their
 * offset across partial writes and wait for writability with poll().
 *
 * Slow consumers: time spent waiting for a peer is charged to its
 * output_stall, shared by all batches of the connection. The connection is
 * given up once it has been waited on for timeout_ms in total without having
 * accepted min_bytes since the budget last started over.
 */
typedef struct output_stall_t {
    long long waited_ms; // time spent waiting for the peer to read, since the last reset
    size_t bytes;        // bytes the peer accepted since the last reset
} output_stall;

typedef struct output_batch_t {
    int fd;
    char *buffer;
    size_t capacity;
    size_t used;
    int timeout_ms;
    size_t min_bytes;    // bytes the peer must accept per timeout_ms of waiting; 0: none
    output_stall *stall; // per-connection budget shared by all batches; NULL: fresh per write
} output_batch;

/** @return 0 when all bytes are written, -1 on error or timeout */
int write_fully(int fd, const char *bytes, size_t len, int timeout_ms);

/** write_fully() charging the wait time to a per-connection budget (see output_batch). */
int write_fully_budget(int fd, const char *bytes, size_t len, int timeout_ms, size_t min_bytes, output_stall *stall);

/** @return 0 on success, -1 if a needed flush/direct write failed */
int output_batch_add(output_batch *batch, const char *bytes, size_t len);

/** @return 0 on success, -1 on error or timeout */
int output_batch_flush(output_batch *batch);

#endif /* OUTPUT_BATCH_H */
