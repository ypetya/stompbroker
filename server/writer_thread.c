#include <unistd.h>
#include <errno.h>
#include "writer_thread.h"
#include "data/message/with_payload_length.h"
#include "../logger.h"
#include "../lib/emalloc.h"
#include "../parse_args.h"

#define MESSAGES_BATCH_SIZE 1000
void ts_dequeue_multiple_messages_for_same_fd(message_with_frame_len* (*ret)[MESSAGES_BATCH_SIZE], ts_queue * q, int buffer_size);

void *writer_thread(void *vargp) {
    ts_queue * output_queue = vargp;

    stomp_app_config * config = config_get_config();

    char write_buffer[config->output_buffer_size];
    message_with_frame_len* msg = NULL;
    message_with_frame_len * messages[MESSAGES_BATCH_SIZE] = {NULL};

    int res = 0, i, buffer_ptr, fd = 0;
    while (YES) {
        res = -1;
        ts_dequeue_multiple_messages_for_same_fd(&messages, output_queue, config->output_buffer_size);

        for (i = 0, buffer_ptr = 0; i < MESSAGES_BATCH_SIZE; i++) {
            msg = messages[i];
            if (msg == NULL) break; //Last item reached, end of messages
            if (msg->fd == -1) {
                debug(" * Writer thread: Poison pill detected.\n");
                message_destroy_with_frame_len(msg);
                return NULL; // exit thread
            }
            debug(">>>\n%s\n", msg->content);
            size_t len = ws_output_filter(msg);
            fd = msg->fd;
#ifdef DEBUG_OUTPUT
            printf("Buffering message for output: fd: %d, len: %d, data:", fd, len);
            for (int j = 0; j < len; j++) {
                if (j < 2 || j == len - 1)printf("%02x", msg->content[j] & 0xff);
                else printf("%c", msg->content[j]);
            }
            printf("\n\n");
#endif
            memcpy(&write_buffer[buffer_ptr], msg->content, len);
            buffer_ptr += len;
            message_destroy_with_frame_len(msg);
        }
        if (buffer_ptr > 0) {
            res = write(fd, write_buffer, buffer_ptr);

            for (int i = 0; i < MESSAGES_BATCH_SIZE; i++)messages[i] = NULL;

#ifdef DEBUG_OUTPUT
            printf("Last wrote was len: %d/%d, data:", res, buffer_ptr);
            for (int j = 0; j < buffer_ptr; j++) {
                if (j < 2 || j == buffer_ptr - 1)printf("%02x", write_buffer[j] & 0xff);
                else printf("%c", write_buffer[j]);
            }
            printf("\n\n");
#endif

            if (res > 0 && res < buffer_ptr) {
#ifdef DEBUG_OUTPUT
                debug("Half frame\n");
#endif
            }
            if (res < 0) {

                //perror("Could not send messages. Client may disconnected");
                //warn("fd:%d\n", fd);

#ifdef DEBUG_OUTPUT
                debug("Problem\n");
                if (errno == EAGAIN) debug("EAGAIN!\n");

#endif
            }
        }

        usleep(10);
    }

    return NULL;
}

void ts_dequeue_multiple_messages_for_same_fd(message_with_frame_len* (*ret)[MESSAGES_BATCH_SIZE], ts_queue * q, int buffer_size) {

    message_with_frame_len * msg = NULL;
    general_list_item * parent = NULL;
    general_list_item * peek_cursor = NULL;
    int first_fd = 0;
    int index = 0;
    int remaining_len = buffer_size;

    pthread_mutex_lock(&q->lock);

    if (peek_cursor = q->q.first) {
        for (;
                index < MESSAGES_BATCH_SIZE && peek_cursor != NULL && remaining_len > 0;
                peek_cursor = peek_cursor->next) {

            msg = (message_with_frame_len *) peek_cursor->data;
            if (msg->fd == -1) {
                (*ret)[index++] = msg;
                unchain_child(&q->q, parent, peek_cursor);
                break;
            } else if (first_fd == 0)
                first_fd = msg->fd; // first item, always pick-up
            else if (msg->fd != first_fd) {
                parent = peek_cursor;
                continue; // skip different fd
            } else if (msg->frame_len > remaining_len) break; // too large msg to fit in
            // pick-up msg
            (*ret)[index++] = msg;
            remaining_len -= msg->frame_len;
            // unchain
            unchain_child(&q->q, parent, peek_cursor);
        }
    }

    pthread_mutex_unlock(&q->lock);
}