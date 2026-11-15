#include "reader_thread.h"
#include "worker_thread_data.h"
#include "../data/message/with_timestamp.h"

void *reader_thread(void *vargp) {
    worker_thread_data_struct * queues = vargp;

    ts_queue * input_queue = queues->input_q;
    ts_queue * output_queue = queues->output_q;
    queue * stale_queue = queues->stale_queue;

    // TODO: dequeue in a batch for speed improvement?
    message_with_timestamp * msg;

    while (YES) {
        if((msg = ts_dequeue(input_queue))){
            if (msg->fd == -1) {
                debug(" * Reader thread: Poison pill detected.\n");
                message_destroy_with_timestamp(msg);
                stomp_stop(stale_queue);
                return NULL; // exit thread
            }

            // The WebSocket handshake is done by the epoll thread (per-connection
            // state); everything on the input queue is a STOMP frame.
            debug("<<<\n%s\n", msg->content);

            int purge_fd = stomp_process(input_queue, stale_queue, output_queue, msg);
            if (purge_fd) {
                // FIXME: dequeue purge_fd 
            }

            message_destroy_with_timestamp(msg);
        }
    }
    
    return NULL;
}