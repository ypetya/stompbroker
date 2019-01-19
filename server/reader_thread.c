#include "reader_thread.h"
#include "process.h"
#include "data/message/with_timestamp.h"
#include "../websocket/filter/handshake.h"

void *reader_thread(void *vargp) {
    worker_thread_data_struct * queues = vargp;

    ts_queue * input_queue = queues->input_q;
    ts_queue * output_queue = queues->output_q;

    while (1) {

        message_with_timestamp * msg = ts_dequeue(input_queue);

        if (msg != NULL) {
            do {
                if (msg->fd == -1) {
                    debug(" * Reader thread: Poison pill detected.\n");
                    message_destroy_with_timestamp(msg);
                    stomp_stop();
                    return NULL; // exit thread
                }

                if (ws_input_filter_handshake(output_queue, msg) == WS_NO_NEED_OF_HANDSHAKE) {
                    debug("<<<\n%s\n", msg->content);

                    stomp_process(input_queue, output_queue, msg);
                }
                message_destroy_with_timestamp(msg);
            } while(msg = ts_dequeue(input_queue));
        }

        usleep(10);
    }

    return NULL;
}