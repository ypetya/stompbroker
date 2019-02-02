#include <unistd.h>
#include <errno.h>
#include "writer_thread.h"
#include "../data/message/with_payload_length.h"
#include "../../logger.h"
#include "../../lib/emalloc.h"
#include "../../lib/general_list.h"
#include "../../lib/thread_safe_queue.h"
#include "../../parse_args.h"

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
            memcpy(&write_buffer[buffer_ptr], msg->content, len);
            buffer_ptr += len;
            message_destroy_with_frame_len(msg);
        }

        if (buffer_ptr > 0) {
            int offset=0;
            do{
                { 
                    int retry_count = 100;
                    // non blocking write attempt
                    do{
                        res = write(fd, &write_buffer[offset], buffer_ptr);
                        if(res<0 && errno == EAGAIN) {
                            usleep(10);
                        }
                    } while(res<0 && --retry_count>0);

                    if(retry_count<=0) {
                        // we have tried to recover, but failed.
                        // FIXME: should gracefully close connection (and purge invalid data from internal queues)
                        close(fd);
                        break;
                    }
                }
                offset+=res;
                buffer_ptr -=res;              
            } while(buffer_ptr>0);

            
            for (int i = 0; i < MESSAGES_BATCH_SIZE; i++)messages[i] = NULL;
        }

        usleep(10);
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

    if (peek_cursor = q->q.first) {
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