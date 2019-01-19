#include "distribute_messages.h"
#include "../data_wrappers/pub_sub.h"
#include <time.h>

char * itoa(int num);

/**
 * message_id is incremented with every sent out message
*/
unsigned int message_id = 0;

void distribute_messages(ts_queue* input_queue, ts_queue* output_queue, 
    message_with_timestamp *input, parsed_message * pm,
    unsigned int ttl) {
    clock_t now = clock();
    clock_t ellapsed = now- input->ts;
    //drop expired message
    if(input->ts != 0 && ttl !=0 && ellapsed > ttl) return;


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

    if(messages_out->size>0) {
        // message consumed
        ts_enqueue_multiple(output_queue, messages_out);
    } else if(ttl>0) {
        // not consumed, and there is a ttl. enqueue it back
        ts_enqueue(input_queue,input);
    }
    aa_free(aa);

    list_clear(messages_out);
    free(messages_out);

    list_clear(matching_clients);
    free(matching_clients);
}

char str_buf[20];

char * itoa(int num) {
    snprintf(str_buf, 20, "%d", num);
    return str_buf;
}