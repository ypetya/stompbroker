#include "stomp.h"
#include "../logger.h"
#include <sys/socket.h>
#include <stdint.h>

#include "../../lib/thread_safe_queue.h"
#include "parser.h"
#include "./data_wrappers//session_storage.h"


ts_queue * global_outputqueue;

void stomp_process(ts_queue* output_queue, message *input) {
    if (global_outputqueue == NULL) global_outputqueue = output_queue;

    int client_id = session_storage_find_client_id(input->fd);
    parsed_message * pm = parse_message(input);
    message * resp = NULL;
   
    switch (pm->command) {
        case FRM_CONNECT_ID:
        {
            int new_id = session_storage_add_new(input->fd);
            if (new_id >= 0)
                resp = message_connected(input->fd, new_id);
            else
                resp = message_error(input->fd, "Can not connect,"
                    " client is already connected!\n");
            break;
        }
        case FRM_DISCONNECT_ID:
        {
            if (client_id >= 0)
                session_storage_remove(client_id);
            else
                resp = message_error(input->fd, "Can not disconnect,"
                    " client is not connected yet!\n");

            break;
        }
        default:
            resp = message_error(input->fd, "Invalid message!\n");
    }

    if (resp != NULL)ts_enqueue(output_queue, resp);

    free_parsed_message(pm);
}

void stomp_start() {
    global_outputqueue = NULL;
    session_storage_init();
}

void stomp_stop() {
    session_storage_dispose(global_outputqueue);
}