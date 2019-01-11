#include "stomp.h"
#include "../logger.h"
#include <sys/socket.h>
#include <stdint.h>

#include "../lib/thread_safe_queue.h"
#include "parser.h"
#include "../server/data/session_storage.h"
#include "data_wrappers/session.h"
#include "./data_wrappers/pub_sub.h"
#include "../lib/associative_array.h"

int message_id = 0;


char str_buf[20];

char * itoa(int num) {
    snprintf(str_buf, 20, "%d", num);
    return str_buf;
}

void stomp_process(ts_queue* output_queue, message *input) {

    int client_id = input->fd;
    int client_id_wo_flags = session_without_flags(input->fd);
    if (client_id != client_id_wo_flags) {
        // checking cmd flags
        if (session_is_cmd_purge(client_id) > 0) {
            int session_with_other_flags = session_wo_cmd_purge(client_id);
            debug("Purge STOMP session fd: %d %s\n",client_id_wo_flags,session_is_encoded(client_id) ? "Websocket": "");
            pubsub_remove_client(session_with_other_flags);
            stomp_session_set_connected(client_id_wo_flags, 0);
            return;
        }
    }
    int client_connected = stomp_session_is_connected(client_id_wo_flags);
    parsed_message * pm = parse_message(input);
    message_with_frame_len * resp = NULL;

    switch (pm->command) {
        case FRM_CONNECT_ID:
        {
            if (client_connected != 0)
                resp = message_error(input->fd, "Can not connect,"
                    " client is already connected!");
            else {
                stomp_session_set_connected(client_id_wo_flags, 1);
                resp = message_connected(input->fd, client_id_wo_flags);
            }
            break;
        }
        case FRM_DISCONNECT_ID:
        {
            if (client_connected > 0) {
                pubsub_remove_client(client_id);
                stomp_session_set_connected(client_id_wo_flags, 0);
            } else
                resp = message_error(input->fd, "Can not disconnect,"
                    " client is not connected yet!");

            break;
        }
        case FRM_SUBSCRIBE_ID:
        {
            if (pm->id == NULL) {
                resp = message_error(input->fd, "No id defined!");
            } else if (pm->topic == NULL)
                resp = message_error(input->fd, "No destination defined!");
            else if (client_connected > 0) {
                pubsub_subscribe(pm->topic, client_id, pm->id);
            } else
                resp = message_error(input->fd, "Not connected!");
            break;
        }
        case FRM_UNSUBSCRIBE_ID:
        {
            if (pm->id == NULL) {
                resp = message_error(input->fd, "No id defined!");
            } else if (pm->topic == NULL)
                resp = message_error(input->fd, "No destination defined!");
            else if (client_connected > 0)
                pubsub_unsubscribe(pm->topic, client_id, pm->id);
            else
                resp = message_error(input->fd, "Not connected!");
            break;
        }
        case FRM_SEND_ID:
        {
            if (pm->topic == NULL)
                resp = message_error(input->fd, "No topic defined!");
            else if (client_connected > 0) {
                if (strchr(pm->topic, '*') != NULL) {
                    resp = message_error(input->fd,
                            "Can not have wildcard in message destination!\n");
                    break;
                };
                general_list * matching_clients = list_new();
                general_list * messages_out = list_new();
                pubsub_find_matching(pm->topic, matching_clients);

                general_list_item * first = matching_clients->first;

                associative_array * aa = emalloc(sizeof (associative_array));

                aa_merge(aa, pm->headers->root);
                aa_put(aa, "content-type", "text/plain");
                aa_put(aa, "content-length", itoa(strlen(pm->message_body)));

                while (first != NULL) {
                    subscription * sub = first->data;
#ifdef DEBUG
                    aa_put(aa, "message-id", itoa(message_id));
#else
                    aa_put(aa, "message-id", itoa(message_id++));
#endif
                    aa_put(aa, "destination", pm->topic);
                    aa_put(aa, "subscription", sub->id);

                    message_with_frame_len * o = message_send_with_headers(sub->session_id,
                            aa,
                            pm->message_body);

                    list_add(messages_out, o);

                    first = first->next;
                }

                ts_enqueue_multiple(output_queue, messages_out);

                aa_free(aa);

                list_clear(messages_out);
                free(messages_out);

                list_clear(matching_clients);
                free(matching_clients);
            } else resp = message_error(input->fd, "Not connected!");

            break;
        }
        case FRM_DIAGNOSTIC_ID:
        { /** This DIAGNOSTIC frame returns internal info*/
            if (pm->message_body == NULL) {
                resp = message_error(input->fd, "Empty message_body!");
                break;
            }
            debug("Diagnostic query %s\n", pm->message_body);
            char buf[10];
            if (strncmp(pm->message_body, "session-size", 12) == 0) {
                sprintf(buf, "%d", session_storage_size());
                resp = message_diagnostic(input->fd, pm->message_body, buf);
            } else if (strncmp(pm->message_body, "session-encoded-size", 71 - 51) == 0) {
                sprintf(buf, "%d", session_storage_encoded_size());
                resp = message_diagnostic(input->fd, pm->message_body, buf);
            } else if (strncmp(pm->message_body, "session-connected-size", 73 - 51) == 0) {
                sprintf(buf, "%llu", stomp_session_connected_size());
                debug("connected %llu\n", stomp_session_connected_size());
                resp = message_diagnostic(input->fd, pm->message_body, buf);
            } else if (strncmp(pm->message_body, "pubsub-size", 11) == 0) {
                sprintf(buf, "%d", pubsub_size());
                resp = message_diagnostic(input->fd, pm->message_body, buf);
            } else if (strncmp(pm->message_body, "subs", 4) == 0) {
                char * large_buffer = emalloc(3500);
                pubsub_to_str(large_buffer, 3500);
                resp = message_diagnostic(input->fd, pm->message_body, large_buffer);
                free(large_buffer);
            } else {
                resp = message_error(input->fd, "Invalid message!");
            }
            break;
        }
        default:
            resp = message_error(input->fd, "Invalid message!");
    }

    if (resp != NULL) {
        ts_enqueue(output_queue, resp);
    } else if (pm->receipt_id != NULL) {
        ts_enqueue(output_queue, message_receipt(input->fd, pm->receipt_id));
    }

    free_parsed_message(pm);
}

void stomp_start() {
    stomp_session_init();
    pubsub_init();
}

void stomp_stop() {
    pubsub_dispose();
    //stomp_session_dispose(); - not needed, no such method exists
}