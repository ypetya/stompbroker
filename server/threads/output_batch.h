#ifndef OUTPUT_BATCH_H
#define OUTPUT_BATCH_H

#include <stddef.h>

/**
 * Batched, bounded socket output for the writer threads.
 *
 * Messages are packed into a fixed size buffer; the buffer is flushed when
 * the next message would not fit, and a message larger than the whole buffer
 * is written directly (never copied past the buffer end). Writes keep their
 * offset across partial writes and wait for writability with poll(); the
 * connection is given up only after timeout_ms without completing.
 */
typedef struct output_batch_t {
    int fd;
    char *buffer;
    size_t capacity;
    size_t used;
    int timeout_ms;
} output_batch;

/** @return 0 when all bytes are written, -1 on error or timeout */
int write_fully(int fd, const char *bytes, size_t len, int timeout_ms);

/** @return 0 on success, -1 if a needed flush/direct write failed */
int output_batch_add(output_batch *batch, const char *bytes, size_t len);

/** @return 0 on success, -1 on error or timeout */
int output_batch_flush(output_batch *batch);

#endif /* OUTPUT_BATCH_H */
