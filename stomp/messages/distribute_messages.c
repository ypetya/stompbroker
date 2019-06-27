#include "distribute_messages.h"
#include "../data_wrappers/pub_sub.h"
#include "../../lib/queue.h"
#include "../../server/data/message/with_payload_length.h"
#include "create_diagnostic_message.h"
#include <time.h>

char * itoa(int num);

void add_next_message_id(associative_array * headers);

general_list * pick_next(general_list * subscriptions);

/**
 * Matches message destination topics against existing subrscriptions. If there 
 * is a match, message is put into the output_queue.
 * If there is no match and TTL > 0, message is put into stale_queue.
 * The message returns trueish if the input message is processed a clone has
 * been put into output and needs to be freed.
 * 
 * supporting load-balancing like message distributions:
 * There is a maintained counter which is increased by every message towards
 * any of the distributed topics. If the modulo of the index of a listeners 
 * matches this counter it will be chosen as the single consumer of the message
 * (sent to the matching distributed topic)
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
    
    if(pubsub_find_matching(pm->topic, matching_clients) > 0) {
        if(*pm->topic == '#' && matching_clients->size > 0) {
            matching_clients = pick_next(matching_clients);
        }

        general_list_item * first = matching_clients->first;

        // Use headers
        associative_array * message_headers = emalloc(sizeof (associative_array));

        aa_merge(message_headers, pm->headers->root);
        aa_put(message_headers, "content-type", "text/plain");
        aa_put(message_headers, "content-length", itoa(strlen(pm->message_body)));
        // Handle special topic, diagnostic messages
        if(strncmp(pm->topic,"DIAG",4)==0) 
            create_diagnostic_headers(message_headers, pm->message_body,
                input_queue, output_queue, stale_queue);

        while (first != NULL) {
            subscription * sub = first->data;
            add_next_message_id(message_headers);
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
    }
    
    return ret;
}

/**
 * Run on stale_q:
 * - drop expired items
 * - collect matching topics
*/
void distribute_messages_from_stale_q(
    char * topic, int client_session_id, char* subscription_id,
    queue * stale_queue,
    ts_queue* output_queue,
    unsigned int configured_ttl
) {
    char * topic_mask_wild_card = strchr(topic, '*');
    int topic_mask_len = topic_mask_wild_card == NULL ?
            strlen(topic) :
            topic_mask_wild_card - topic;

    // 1) topic -> pick-up all the messages
    general_list_item * parent = NULL;
    general_list_item * processed_item = NULL;
    general_list_item * q_item = stale_queue->first;
    general_list * messages_out = list_new();

    clock_t now = clock();

    // 2) unchain all matching topics
    while(q_item) {
        parsed_message * pm = (parsed_message *)q_item->data;
        processed_item = q_item;

        clock_t ellapsed = now- pm->ts;
    
        // Drop TTL expired message
        if(pm->ts != 0 && configured_ttl !=0 && ellapsed > configured_ttl) {
            unchain_child(stale_queue, parent, processed_item);
            q_item = processed_item->next;
            
            free_parsed_message(pm);
            free(processed_item);
            continue;
        }

        int message_topic_len = strlen(pm->topic);
        if ((topic_mask_wild_card == NULL && message_topic_len == topic_mask_len
            || topic_mask_wild_card != NULL && topic_mask_len < message_topic_len) && 
            strncmp(pm->topic, topic, topic_mask_len)==0) {
            unchain_child(stale_queue, parent, processed_item);
            // 3A) generate output message

            associative_array * message_headers = emalloc(sizeof (associative_array));

            aa_merge(message_headers, pm->headers->root);
            aa_put(message_headers, "content-type", "text/plain");
            aa_put(message_headers, "content-length", itoa(strlen(pm->message_body)));
            add_next_message_id(message_headers);
            aa_put(message_headers, "destination", topic);
            aa_put(message_headers, "subscription", subscription_id);
            
            message_with_frame_len * o = message_send_with_headers(client_session_id,
                    message_headers,
                    pm->message_body);

            list_add(messages_out, o);
            
            aa_free(message_headers);

            q_item = processed_item->next;
            
            free_parsed_message(pm);
            free(processed_item);
            continue;
        }
        parent = q_item;
        q_item = parent->next;
    }

    // 3B) Deliver messages
     if(messages_out->size>0) 
        ts_enqueue_multiple(output_queue, messages_out);

    // 4) free-up parsed messages
    list_clear(messages_out);
    free(messages_out);
}

char str_buf[20];

char * itoa(int num) {
    snprintf(str_buf, 20, "%d", num);
    return str_buf;
}

/**
 * message_id is incremented with every sent out message
*/
unsigned int message_id = 0;

void add_next_message_id(associative_array * headers){
#ifdef DEBUG
    aa_put(headers, "message-id", itoa(message_id));
#else
    aa_put(headers, "message-id", itoa(message_id++));
#endif
}


unsigned int next_counter = 0;

general_list * pick_next(general_list * subscriptions) {
    general_list * messages_out = list_new();
    
    int next_index = next_counter++ % subscriptions->size;
    
    void * data = list_unchain_at(subscriptions, next_index);
    
    list_add(messages_out, data);
        
    list_clear(subscriptions);
    free(subscriptions);
    
    return messages_out;
}