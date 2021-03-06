#include "stomp.h"
#include "../logger.h"

#include "../parse_args.h"

#include "../lib/thread_safe_queue.h"
#include "../server/data/session_storage.h"
#include "data_wrappers/session.h"
#include "parser.h"

#include "messages/message.h"
#include "messages/create_diagnostic_message.h"
#include "messages/distribute_messages.h"

/**
 * Time to live, from config at startup
 * */
unsigned int ttl;
unsigned int stale_queue_max_size;

int check_stomp_version(parsed_message * pm);
/**
 * Processing incoming STOMP message
 * 
 * @return fd: to do cleanup for. 0: message processed
*/
int stomp_process(ts_queue* input_queue, 
    queue* stale_queue, 
    ts_queue* output_queue, 
    message_with_timestamp *input) {
    
    int message_consumed = STOMP_MESSAGE_CONSUMED;
    int client_id = input->fd;
    int client_id_wo_flags = session_without_flags(input->fd);
    if (client_id != client_id_wo_flags) {
        // checking cmd flags
        if (session_is_cmd_purge(client_id) > 0) {
            int session_with_other_flags = session_wo_cmd_purge(client_id);
            debug("Purge STOMP session fd: %d %s\n",client_id_wo_flags,session_is_encoded(client_id) ? "Websocket": "");
            pubsub_remove_client(session_with_other_flags);
            stomp_session_set_connected(client_id_wo_flags, 0);
            
            return client_id_wo_flags;
        }
    }
    int client_connected = stomp_session_is_connected(client_id_wo_flags);
    parsed_message * pm = parse_message(input);
    message_with_frame_len * resp = NULL;

    switch (pm->command) {
        case FRM_CONNECT_ID:
        {
            if (client_connected > 0){
                resp = message_error(input->fd, "Can not connect,"
                    " client is already connected!");
            } else /*if(check_stomp_version(pm)) */{
                stomp_session_set_connected(client_id_wo_flags, 1);
                resp = message_connected(input->fd, client_id_wo_flags);
            } 

            // Although the specification wants a version header, the Stompjs does not support this!
            //
            // else {
            //     resp = message_error(input->fd, "Can not connect,"
            //         " STOMP version 1.2 is not supported by client!");            
            // }
            
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
                distribute_messages_from_stale_q(pm->topic, client_id, pm->id, stale_queue,output_queue, ttl);
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
                // Do not allow wildcard topic
                if (strchr(pm->topic, '*') != NULL) {
                    resp = message_error(input->fd,
                            "Can not have wildcard in message destination!\n");
                    break;
                };
                
                message_consumed = distribute_messages(input_queue, 
                    stale_queue_max_size,
                    stale_queue,
                    output_queue, input, pm, ttl);

            } else resp = message_error(input->fd, "Not connected!");

            break;
        }
        case FRM_DIAGNOSTIC_ID:
        { /** This DIAGNOSTIC frame returns internal info */
            if (pm->message_body == NULL) {
                resp = message_error(input->fd, "Empty message_body!");
                break;
            }
            resp = create_diagnostic_message(input,pm, input_queue, output_queue);
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

    if(message_consumed) free_parsed_message(pm);

    return 0;
}

void stomp_start() {
    stomp_app_config* config = config_get_config();
    ttl = config->ttl;
    stale_queue_max_size = config->max_stale_queue_size;

    stomp_session_init();
    pubsub_init();
}

void stomp_stop(queue * stale_queue) {
    pubsub_dispose();
    //free-up stale_queue
    general_list_item* c = stale_queue->first;
    while(c!=NULL) {
        free_parsed_message(c->data);
        c=c->next;
    }
    //stomp_session_dispose(); - not needed, no such method exists
}

/**
 * @return 1: if "1.2" is present in header "accept-version"; 0: otherwise
*/
int check_stomp_version(parsed_message * pm) {
    aa_item* header = aa_get(pm->headers->root, "accept-version");
    if(header) {
        if(strstr(header->value,"1.2")) return 1;
    }
    return 0;
}