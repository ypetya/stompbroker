#include <stdlib.h>
#include <string.h>
#include "pub_sub.h"
#include "../../../lib/emalloc.h"
#include "../../../lib/clone_str.h"

// TODO: introduce avl-tree for better match ratio
general_list * topics;

void pubsub_init() {
    topics = emalloc(sizeof (general_list));
}

void pubsub_dispose() {
    if (topics->size > 0) {
        general_list_item * c = topics->first;
        general_list_item * n = topics->first->next;
        while (c != NULL) {
            n = c->next;

            subscription * sub = c->data;
            free(sub->topic_pattern);
            //free(sub);
            //free(c);
            c = n;
        }
    }

    list_free(topics);
    topics = NULL;
}

void pubsub_subscribe(char* topic_str, int client_session_id, int connection_id) {
    if (topics == NULL) return;
    subscription * new_sub = emalloc(sizeof (subscription));
    new_sub->session_id = client_session_id;
    new_sub->client_id = connection_id;
    new_sub->topic_pattern = clone_str(topic_str);

    list_add(topics, new_sub);
}

void pubsub_unsubscribe(char* topic_str, int session_id, int external_client_id) {
    subscription * sub = emalloc(sizeof (subscription));
    sub->session_id = session_id;
    sub->client_id = external_client_id;
    sub->topic_pattern = clone_str(topic_str);

    int ix = -1;

    while ((ix = list_index_of(topics, sub)) >= 0) {
        subscription * found = list_unchain_at(topics, ix);
        free(found->topic_pattern);
        free(found);
    }

    free(sub->topic_pattern);
    free(sub);
}

void pubsub_remove_client(int client_session_id) {
    if (topics->size == 0) return;

    general_list_item * p = NULL;
    general_list_item * c = topics->first;

    while (c != NULL) {
        subscription * sub = c->data;

        if (sub->session_id == client_session_id) {
            free(sub->topic_pattern);
            free(sub); // = c->data
            //unchain
            if (p == NULL) // first item!
                topics->first = c->next;
            else
                p->next = c->next;
            // last item!
            if (topics->last == c) topics->last = p;

            free(c);

            if (p == NULL)
                c = topics->first;
            else
                c = p->next;

            topics->size = topics->size - 1;
        } else {
            p = c;
            c = c->next;
        }
    }
}

void pubsub_find_matching(char* topic_mask, general_list * matches) {

    if (topics->size == 0) return;

    char * topic_mask_wild_card = strchr(topic_mask, '*');
    int len = topic_mask_wild_card == NULL ?
            strlen(topic_mask) :
            topic_mask_wild_card - topic_mask;

    int len2;

    general_list_item * c = topics->first;

    while (c != NULL) {
        subscription * sub = c->data;

        topic_mask_wild_card = strchr(sub->topic_pattern, '*');
        len2 = topic_mask_wild_card == NULL ?
                strlen(sub->topic_pattern) :
                topic_mask_wild_card - sub->topic_pattern;
        // MIN
        len = len < len2 ? len : len2;
        
        if (strncmp(topic_mask, sub->topic_pattern, len) == 0) {
            list_add(matches, sub);
        }

        c = c->next;
    }
}

int pubsub_size() {
    return topics->size;
}