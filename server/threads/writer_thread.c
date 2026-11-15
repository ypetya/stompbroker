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
#include <pthread.h>

#define MESSAGES_BATCH_SIZE 1000
void ts_dequeue_multiple_messages_for_same_fd(message_with_frame_len* (*ret)[MESSAGES_BATCH_SIZE], ts_queue * q, int buffer_size);
void w_close_connection(int conn_sock, int fd_with_session, ts_queue * input_queue, const char * reason);

/*
 * Several writer threads may hold batches for the same fd. Writing a batch
 * can now take a while (partial writes + waiting for writability), so a
 * striped per-fd lock keeps two batches from interleaving on one socket.
 */
#define WRITE_LOCK_STRIPES 256
static pthread_mutex_t write_locks[WRITE_LOCK_STRIPES];
static pthread_once_t write_locks_once = PTHREAD_ONCE_INIT;

static void init_write_locks(void) {
    for (int i = 0; i < WRITE_LOCK_STRIPES; i++) pthread_mutex_init(&write_locks[i], NULL);
}

static pthread_mutex_t * write_lock_of(int fd) {
    return &write_locks[(unsigned int) fd % WRITE_LOCK_STRIPES];
}

void *writer_thread(void *vargp) {
    worker_thread_data_struct * queues = vargp;

    ts_queue * input_queue = queues->input_q;
    ts_queue * output_queue = queues->output_q;
    //queue * stale_queue = queues->stale_queue;

    stomp_app_config * config = config_get_config();
    pthread_once(&write_locks_once, init_write_locks);

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
        ts_dequeue_multiple_messages_for_same_fd(&messages, output_queue, config->output_buffer_size);

        output_batch batch = {
            .fd = 0,
            .buffer = write_buffer,
            .capacity = (size_t) config->output_buffer_size,
            .used = 0,
            .timeout_ms = config->write_timeout_ms
        };
        pthread_mutex_t * lock = NULL;

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
            if (lock == NULL) {
                lock = write_lock_of(fd);
                pthread_mutex_lock(lock);
                batch.fd = fd;
            }
            debug(">>>\n%s\n", msg->content);
            close_after |= msg->close_after;
            size_t len = ws_output_filter(msg);
            failed = output_batch_add(&batch, msg->content, len) < 0;
            message_destroy_with_frame_len(msg);
        }

        if (!failed) failed = output_batch_flush(&batch) < 0;
        if (lock != NULL) pthread_mutex_unlock(lock);

        if (failed) // slow consumer (timeout) or broken connection
            w_close_connection(fd, fd_with_session, input_queue, "drop blocked connection");
        else if (close_after) // e.g. WebSocket close frame sent: now close TCP
            w_close_connection(fd, fd_with_session, input_queue, "closing connection after close frame");
    }

    return NULL;
}

void ts_dequeue_multiple_messages_for_same_fd(message_with_frame_len* (*ret)[MESSAGES_BATCH_SIZE], ts_queue * q, int buffer_size) {

    message_with_frame_len * msg = NULL;
    general_list_item * parent = NULL;
    general_list_item * current = NULL;
    general_list_item * peek_cursor = NULL;
    int first_fd = 0;
    int index = 0;
    int remaining_len = buffer_size;

    pthread_mutex_lock(&q->lock);
    
    while(q->q.size==0)
        pthread_cond_wait(&q->has_new_elements,&q->lock);
    
    if ((peek_cursor = q->q.first)) {
        while(index < MESSAGES_BATCH_SIZE && peek_cursor != NULL && remaining_len > 0) {
            current = peek_cursor;
            msg = (message_with_frame_len *) peek_cursor->data;
            if (msg->fd == -1) {
                (*ret)[index++] = msg;
                unchain_child(&q->q, parent, peek_cursor);
                free(peek_cursor);
                break;
            } else if (first_fd == 0)
                first_fd = msg->fd; // first item, always pick-up
            else if (msg->fd != first_fd) {
                parent = peek_cursor;
                peek_cursor = peek_cursor->next;
                continue; // skip different fd
            } else if (msg->frame_len > remaining_len) break; // too large msg to fit in
            // pick-up msg
            (*ret)[index++] = msg;
            remaining_len -= msg->frame_len;
            // unchain
            unchain_child(&q->q, parent, peek_cursor);
            peek_cursor = peek_cursor->next;
            free(current);
        }
    }

    pthread_mutex_unlock(&q->lock);
}

void w_close_connection(int conn_sock, int fd_with_session, ts_queue * input_queue, const char * reason) {
    info("server: %s. fd:%d\n", reason, conn_sock);

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