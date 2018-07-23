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
    list_free(topics);
}

void pubsub_subscribe(char* topic_str, int client_session_id, int connection_id) {
    subscription * new_sub = emalloc(sizeof (subscription));
    new_sub->session_id = client_session_id;
    new_sub->client_id = connection_id;
    new_sub->topic_pattern = clone_str(topic_str);

    list_add(topics, new_sub);
}

void pubsub_unsubscribe(char* topic_str, int session_id, int client_id) {
    subscription * sub = emalloc(sizeof (subscription));
    sub->session_id = session_id;
    sub->client_id = client_id;
    sub->topic_pattern = clone_str(topic_str);

    int ix = -1;

    while ((ix = list_index_of(topics, sub)) >= 0) list_remove_at(topics, ix);

    free(sub);
}

void pubsub_find_matching(char* topic_mask, general_list * matches) {

    if (topics->size == 0) return;

    general_list_item * c = topics->list;

    while (c != NULL) {
        subscription * sub = c->data;

        // TODO: introduce pattern matching
        if (strcmp(topic_mask, sub->topic_pattern) == 0) {
            list_add(matches, sub);
        }

        c = c->next;
    }
}

int pubsub_size() {
    return topics->size;
}