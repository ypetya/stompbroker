#include "reader_thread.h"
#include "process.h"

void *reader_thread(void *vargp) {
    worker_thread_data_struct * queues = vargp;

    ts_queue * input_queue = queues->input_q;
    ts_queue * output_queue = queues->output_q;

    while (YES) {

        message * msg = ts_dequeue(input_queue);

        if (msg != NULL) {
            if (msg->fd == -1) {
                debug(" * Reader thread: Poison pill detected.\n");
                message_destroy(msg);
                stomp_stop();
                break;
            }

            if (ws_input_filter_handshake(output_queue, msg) == WS_NO_NEED_OF_HANDSHAKE) {
                debug("<<<\n%s\n", msg->content);

                stomp_process(output_queue, msg);
            }
            message_destroy(msg);
        }

        usleep(10);
    }

    return NULL;
}