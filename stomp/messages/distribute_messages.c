#include "distribute_messages.h"
#include "../data_wrappers/pub_sub.h"
#include "../../server/data/message/with_payload_length.h"
#include "create_diagnostic_message.h"
#include <time.h>

char * itoa(int num);

/**
 * message_id is incremented with every sent out message
*/
unsigned int message_id = 0;

/**
 * Matches message destination topics against existing subrscriptions. If there is a match, message is pup into output_queue.
 * If there is no match and TTL > 0, message is put into stale_queue.
 * The message returns trueish if the input message is processed a clone has been put into output and needs to be freed.
 * 
 * @return 1 if message consumed, 0 if not consumed
*/
int distribute_messages(
    ts_queue* input_queue,
    unsigned max_stale_queue_size,
    queue * stale_queue,
    ts_queue* output_queue, 
    message_with_timestamp *input,
    parsed_message * pm,
    unsigned int ttl) {
    int ret = STOMP_MESSAGE_CONSUMED;

    clock_t now = clock();
    clock_t ellapsed = now- input->ts;
    // Drop TTL expired message
    if(input->ts != 0 && ttl !=0 && ellapsed > ttl) return ret;

    // Find matching subscribers
    general_list * matching_clients = list_new();
    general_list * messages_out = list_new();
    pubsub_find_matching(pm->topic, matching_clients);

    general_list_item * first = matching_clients->first;

    // Use headers
    associative_array * message_headers = emalloc(sizeof (associative_array));

    aa_merge(message_headers, pm->headers->root);
    aa_put(message_headers, "content-type", "text/plain");
    aa_put(message_headers, "content-length", itoa(strlen(pm->message_body)));
    // Handle special topic, diagnostic messages
    if(strncmp(pm->topic,"DIAG",4)==0) 
        create_diagnostic_headers(message_headers,pm->message_body,input_queue,output_queue);

    while (first != NULL) {
        subscription * sub = first->data;
#ifdef DEBUG
        aa_put(message_headers, "message-id", itoa(message_id));
#else
        aa_put(message_headers, "message-id", itoa(message_id++));
#endif
        aa_put(message_headers, "destination", pm->topic);
        aa_put(message_headers, "subscription", sub->id);

        message_with_frame_len * o = message_send_with_headers(sub->session_id,
                message_headers,
                pm->message_body);

        list_add(messages_out, o);

        first = first->next;
    }

    if(messages_out->size>0) {
        // message consumed
        ts_enqueue_multiple(output_queue, messages_out);
        // ret = STOMP_MESSAGE_CONSUMED (default)
    } else if(ttl>0) {
        // not consumed, and there is a ttl. enqueue it to stale_queue
        if(enqueue_limited(stale_queue, pm, max_stale_queue_size)>=0)
            ret = STOMP_MESSAGE_NOT_CONSUMED;
    }
    aa_free(message_headers);

    list_clear(messages_out);
    free(messages_out);

    list_clear(matching_clients);
    free(matching_clients);

    return ret;
}

void distribute_messages_from_stale_q(
    char * topic,
    queue * stale_queue,
    ts_queue* output_queue
) {
    // FIXME: implement:
    // 1) topic -> pick-up all the messages
    // 2) unchain all
    // 3) generate output messages
    // 4) free-up parsed messages
}


char str_buf[20];

char * itoa(int num) {
    snprintf(str_buf, 20, "%d", num);
    return str_buf;
}