#include "stomp.h"
#include "../logger.h"
#include <sys/socket.h>
#include <stdint.h>

#include "../../lib/thread_safe_queue.h"
#include "parser.h"
#include "./data_wrappers/session_storage.h"
#include "./data_wrappers/pub_sub.h"

int message_id = 0;

void stomp_process(ts_queue* output_queue, message *input) {

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
            if (client_id >= 0) {
                session_storage_remove(client_id);
                debug("Disconnect receipt-id:%s\n", pm->receipt_id);
            } else
                resp = message_error(input->fd, "Can not disconnect,"
                    " client is not connected yet!\n");

            break;
        }
        case FRM_SUBSCRIBE_ID:
        {
            if (client_id >= 0) {
                pubsub_subscribe(pm->topic, client_id, pm->id);
            } else resp = message_error(input->fd, "Not connected!\n");
            break;
        }
        case FRM_UNSUBSCRIBE_ID:
        {
            if (client_id >= 0) {
                pubsub_unsubscribe(pm->topic, client_id, pm->id);
            } else resp = message_error(input->fd, "Not connected!\n");
            break;
        }
        case FRM_SEND_ID:
        {
            if (client_id >= 0) {
                general_list clients;
                pubsub_find_matching(pm->topic, &clients);

                general_list_item * first = clients.list;
                while (first != NULL) {
                    subscription * sub = first->data;
                    message * o = message_send(input->fd,
                            sub->client_id,
                            message_id++,
                            pm->topic,
                            pm->message_body
                            );
                    ts_enqueue(output_queue, o);

                    first = first->next;
                }
            } else resp = message_error(input->fd, "Not connected!\n");
            break;
        }
        case FRM_DIAGNOSTIC_ID:
        { /** This DIAGNOSTIC frame returns internal info*/
            if(pm->message_body==NULL) {
                resp = message_error(input->fd, "Empty message_body!\n");
                break;
            }
            debug("Diagnostic query %s\n", pm->message_body);
            char buf[10];
            if (strncmp(pm->message_body, "session-size",12) == 0) {
                sprintf(buf, "%d", session_storage_size());
            } else if (strncmp(pm->message_body, "pubsub-size",11) == 0) {
                sprintf(buf, "%d", session_storage_size());
            } else break;
            resp = message_diagnostic(input->fd, pm->message_body, buf);
            break;
        }
        default:
            resp = message_error(input->fd, "Invalid message!\n");
    }

    if (resp != NULL) {
        ts_enqueue(output_queue, resp);
    } else if (pm->receipt_id != NULL) {
        ts_enqueue(output_queue, message_receipt(input->fd, pm->receipt_id));
    }

    free_parsed_message(pm);
}

void stomp_start() {
    session_storage_init();
    pubsub_init();
}

void stomp_stop(ts_queue* output_queue) {
    pubsub_dispose();
    session_storage_dispose(output_queue);
}