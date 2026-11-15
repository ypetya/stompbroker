#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include "writer_thread.h"
#include "../data/message/with_payload_length.h"
#include "../../logger.h"
#include "../../lib/constants.h"
#include "../../lib/emalloc.h"
#include "../../lib/general_list.h"
#include "../../lib/thread_safe_queue.h"
#include "../../parse_args.h"
#include "../../websocket/filter/dataframe.h"
#include "worker_thread_data.h"
#include "../data/session_storage.h"
#include "output_batch.h"
#include "output_queue.h"
#include <pthread.h>

/*
 * output_queue lets only one writer own an fd at a time, so two batches of
 * one connection are never written concurrently, and the per-connection slow
 * consumer budget (output_queue_stall_of) needs no lock of its own.
 *
 * Writers never close an fd: they shut the socket down, the epoll thread
 * reads EOF and runs connection_close() (see cleanup.h). The fd number cannot
 * be reused before that, and connection_close waits for the owning writer.
 */
static void request_close(int fd, const char * reason) {
    info("server: %s. fd:%d\n", reason, fd);
    shutdown(fd, SHUT_RDWR);
}

/** Is msg still for the session that currently uses its fd? */
static int is_current_session(const message_with_frame_len * msg, int fd) {
    return session_storage_get(fd) && msg->generation == session_storage_generation(fd);
}

int writer_send_batch(message_with_frame_len *(*messages)[MESSAGES_BATCH_SIZE], output_batch *batch) {
    int close_after = 0, failed = 0;

    for (int i = 0; i < MESSAGES_BATCH_SIZE && (*messages)[i] != NULL; i++) {
        message_with_frame_len * msg = (*messages)[i];
        int fd = session_without_flags(msg->fd);

        if (msg->fd == -1) { // poison pill
            debug(" * Writer thread: Poison pill detected.\n");
            message_destroy_with_frame_len(msg);
            return 1;
        }
        if (failed || !is_current_session(msg, fd)) { // disconnected client or closed session
            message_destroy_with_frame_len(msg);
            continue;
        }
        batch->fd = fd;
        debug(">>>\n%s\n", msg->content);
        close_after |= msg->close_after;
        size_t len = ws_output_filter(msg);
        failed = output_batch_add(batch, msg->content, len) < 0;
        message_destroy_with_frame_len(msg);
    }

    if (batch->fd == 0) return 0; // nothing to write
    if (!failed) failed = output_batch_flush(batch) < 0;

    if (failed) // slow consumer (timeout) or broken connection
        request_close(batch->fd, "drop blocked connection");
    else if (close_after) // ERROR, DISCONNECT or WebSocket close sent: now close TCP
        request_close(batch->fd, "closing connection after its last message");
    return 0;
}

void *writer_thread(void *vargp) {
    worker_thread_data_struct * queues = vargp;
    ts_queue * output_queue = queues->output_q;

    stomp_app_config * config = config_get_config();

    char write_buffer[config->output_buffer_size];
    message_with_frame_len * messages[MESSAGES_BATCH_SIZE] = {NULL};

    while (YES) {
        for (int i = 0; i < MESSAGES_BATCH_SIZE; i++) messages[i] = NULL;
        int owned_fd = output_queue_take_batch(&messages, output_queue, config->output_buffer_size);

        output_batch batch = {
            .fd = 0,
            .buffer = write_buffer,
            .capacity = (size_t) config->output_buffer_size,
            .used = 0,
            .timeout_ms = config->write_timeout_ms,
            // a stalled peer must take at least one buffer per write_timeout of waiting
            .min_bytes = (size_t) config->output_buffer_size,
            .stall = owned_fd > 0 ? output_queue_stall_of(owned_fd) : NULL
        };

        if (writer_send_batch(&messages, &batch)) return NULL; // poison pill: exit thread

        output_queue_release_fd(output_queue, owned_fd); // next batch of this connection may go now
    }

    return NULL;
}
