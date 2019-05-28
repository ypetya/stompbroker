#include "reader_thread.h"
#include "worker_thread_data.h"
#include "../data/message/with_timestamp.h"
#include "../../websocket/filter/handshake.h"

void *reader_thread(void *vargp) {
    worker_thread_data_struct * queues = vargp;

    ts_queue * input_queue = queues->input_q;
    ts_queue * output_queue = queues->output_q;
    queue * stale_queue = queues->stale_queue;

    // TODO: dequeue in a batch for speed improvement?
    message_with_timestamp * msg;

    while (msg = ts_dequeue(input_queue)) {

        if (msg->fd == -1) {
            debug(" * Reader thread: Poison pill detected.\n");
            message_destroy_with_timestamp(msg);
            stomp_stop(stale_queue);
            return NULL; // exit thread
        }

        if (ws_input_filter_handshake(output_queue, msg) == WS_NO_NEED_OF_HANDSHAKE) {
            debug("<<<\n%s\n", msg->content);

            int purge_fd = stomp_process(input_queue, stale_queue, output_queue, msg);
            if (purge_fd) {
                // FIXME: dequeue purge_fd 
            }
        }

        message_destroy_with_timestamp(msg);
    }

    return NULL;
}