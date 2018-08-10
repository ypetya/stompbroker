#include "stomp.h"
#include "../logger.h"
#include <sys/socket.h>
#include <stdint.h>

#include "../lib/thread_safe_queue.h"
#include "parser.h"
#include "./data_wrappers/session_storage.h"
#include "./data_wrappers/pub_sub.h"

int message_id = 0;

void stomp_process(ts_queue* output_queue, message *input) {

    int client_id = session_storage_fetch_client_id(input->fd);
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
                pubsub_remove_client(client_id);
                session_storage_remove(client_id);
            } else
                resp = message_error(input->fd, "Can not disconnect,"
                    " client is not connected yet!\n");

            break;
        }
        case FRM_SUBSCRIBE_ID:
        {
            if (pm->topic == NULL)
                resp = message_error(input->fd, "No topic defined!\n");
            else if (client_id >= 0)
                pubsub_subscribe(pm->topic, client_id, pm->id);
            else
                resp = message_error(input->fd, "Not connected!\n");
            break;
        }
        case FRM_UNSUBSCRIBE_ID:
        {
            if (pm->topic == NULL)
                resp = message_error(input->fd, "No topic defined!\n");
            else if (client_id >= 0)
                pubsub_unsubscribe(pm->topic, client_id, pm->id);
            else
                resp = message_error(input->fd, "Not connected!\n");
            break;
        }
        case FRM_SEND_ID:
        {
            if (pm->topic == NULL)
                resp = message_error(input->fd, "No topic defined!\n");
            else if (client_id >= 0) {
                if (strchr(pm->topic, '*') != NULL) {
                    resp = message_error(input->fd,
                            "Can not have wildcard in message destination!\n");
                    break;
                };
                general_list * matching_clients = list_new();
                general_list * messages_out = list_new();
                pubsub_find_matching(pm->topic, matching_clients);

                general_list_item * first = matching_clients->first;
                while (first != NULL) {
                    subscription * sub = first->data;
                    int fd = session_storage_fetch_external_id(sub->session_id);
                    message * o = message_send(
                            fd,
                            sub->client_id,
#ifdef DEBUG
                            message_id,
#else
                            message_id++,
#endif
                            pm->topic,
                            pm->message_body
                            );
                    list_add(messages_out, o);

                    first = first->next;
                }

                ts_enqueue_multiple(output_queue, messages_out);

                list_clear(messages_out);
                free(messages_out);

                list_clear(matching_clients);
                free(matching_clients);
            } else resp = message_error(input->fd, "Not connected!\n");

            break;
        }
        case FRM_DIAGNOSTIC_ID:
        { /** This DIAGNOSTIC frame returns internal info*/
            if (pm->message_body == NULL) {
                resp = message_error(input->fd, "Empty message_body!\n");
                break;
            }
            debug("Diagnostic query %s\n", pm->message_body);
            char buf[10];
            if (strncmp(pm->message_body, "session-size", 12) == 0) {
                sprintf(buf, "%d", session_storage_size());
                resp = message_diagnostic(input->fd, pm->message_body, buf);
            } else if (strncmp(pm->message_body, "pubsub-size", 11) == 0) {
                sprintf(buf, "%d", pubsub_size());
                resp = message_diagnostic(input->fd, pm->message_body, buf);
            } else if (strncmp(pm->message_body, "subs", 4) == 0) {

            } else {
                resp = message_error(input->fd, "Invalid message!\n");
            }
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