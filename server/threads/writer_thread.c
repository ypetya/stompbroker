#include <unistd.h>
#include <errno.h>
#include "writer_thread.h"
#include "../data/message/with_payload_length.h"
#include "../../logger.h"
#include "../../lib/emalloc.h"
#include "../../lib/general_list.h"
#include "../../lib/thread_safe_queue.h"
#include "../../parse_args.h"
#include "worker_thread_data.h"
#include "../data/cleanup.h"
#include "../data/message/with_timestamp.h"
#include "output_batch.h"
#include "output_queue.h"
#include <pthread.h>

void w_close_connection(int conn_sock, int fd_with_session, ts_queue * input_queue, const char * reason);

/*
 * Per-connection slow consumer budget (see output_batch.h). No lock needed:
 * output_queue lets only one writer own an fd at a time, and taking or
 * releasing ownership goes through the queue mutex.
 *
 * That ownership also replaces the former striped write locks: two batches
 * of one connection can no longer be written concurrently, and a slow peer
 * keeps only its own writer waiting instead of every fd sharing its stripe.
 */
static output_stall connection_stalls[FD_MASK + 1];

void *writer_thread(void *vargp) {
    worker_thread_data_struct * queues = vargp;

    ts_queue * input_queue = queues->input_q;
    ts_queue * output_queue = queues->output_q;
    //queue * stale_queue = queues->stale_queue;

    stomp_app_config * config = config_get_config();

    char write_buffer[config->output_buffer_size];
    message_with_frame_len* msg = NULL;
    message_with_frame_len * messages[MESSAGES_BATCH_SIZE] = {NULL};

    int i, fd, fd_with_session, close_after, failed;
    while (YES) {
        fd = 0;
        fd_with_session = 0;
        close_after = 0;
        failed = 0;

        for (int i = 0; i < MESSAGES_BATCH_SIZE; i++)messages[i] = NULL;
        int owned_fd = output_queue_take_batch(&messages, output_queue, config->output_buffer_size);

        output_batch batch = {
            .fd = 0,
            .buffer = write_buffer,
            .capacity = (size_t) config->output_buffer_size,
            .used = 0,
            .timeout_ms = config->write_timeout_ms,
            // a stalled peer must take at least one buffer per write_timeout of waiting
            .min_bytes = (size_t) config->output_buffer_size,
            .stall = owned_fd > 0 ? &connection_stalls[owned_fd] : NULL
        };

        for (i = 0; i < MESSAGES_BATCH_SIZE; i++) {
            msg = messages[i];
            if (msg == NULL) break; //Last item reached, end of messages
            fd_with_session = msg->fd;
            fd = session_without_flags(fd_with_session);
            
            if (fd_with_session == -1) {
                // poison pill
                debug(" * Writer thread: Poison pill detected.\n");
                message_destroy_with_frame_len(msg);
                return NULL; // exit thread
            } else if(failed || !session_storage_get(fd)){ 
                // disconnected client 
                message_destroy_with_frame_len(msg);
                continue; 
            }
            batch.fd = fd;
            debug(">>>\n%s\n", msg->content);
            close_after |= msg->close_after;
            size_t len = ws_output_filter(msg);
            failed = output_batch_add(&batch, msg->content, len) < 0;
            message_destroy_with_frame_len(msg);
        }

        if (!failed) failed = output_batch_flush(&batch) < 0;

        if (failed) // slow consumer (timeout) or broken connection
            w_close_connection(fd, fd_with_session, input_queue, "drop blocked connection");
        else if (close_after) // e.g. WebSocket close frame sent: now close TCP
            w_close_connection(fd, fd_with_session, input_queue, "closing connection after close frame");

        output_queue_release_fd(output_queue, owned_fd); // next batch of this connection may go now
    }

    return NULL;
}

void w_close_connection(int conn_sock, int fd_with_session, ts_queue * input_queue, const char * reason) {
    info("server: %s. fd:%d\n", reason, conn_sock);
    connection_stalls[session_without_flags(conn_sock)] = (output_stall) {0};

    int fd_with_flags = session_set_cmd_purge(conn_sock);
    char * CMD = clone_str("CMD");

    message_with_timestamp * incoming_message = message_create_with_timestamp(
        fd_with_flags,CMD,0);

    free(CMD);
    ts_put_head(input_queue, incoming_message);

    session_storage_lock();
    clean_by_fd(conn_sock);
    session_storage_unlock();
}