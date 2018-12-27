#include <stdlib.h>
#include <string.h>
#include "pub_sub.h"
#include "../../lib/emalloc.h"
#include "../../lib/clone_str.h"

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
            free(sub->id);
            //free(sub);
            //free(c);
            c = n;
        }
    }

    list_free(topics);
    topics = NULL;
}

void pubsub_subscribe(char* topic_str, int client_session_id, char* subscription_id) {
    if (topics == NULL) return;
    subscription * new_sub = emalloc(sizeof (subscription));
    new_sub->session_id = client_session_id;
    new_sub->topic_pattern = clone_str(topic_str);
    new_sub->id = clone_str(subscription_id);

    list_add(topics, new_sub);
}

void pubsub_unsubscribe(char* topic_str, int client_session_id, char* subscription_id) {
    if (topics->size == 0) return;

    general_list_item * p = NULL;
    general_list_item * c = topics->first;

    while (c != NULL) {
        subscription * sub = c->data;

        if (sub->session_id == client_session_id &&
                strcmp(sub->topic_pattern, topic_str) == 0 &&
                strcmp(sub->id, subscription_id) == 0
                ) {
            free(sub->topic_pattern);
            free(sub->id);
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

void pubsub_remove_client(int client_session_id) {
    if (topics->size == 0) return;

    general_list_item * p = NULL;
    general_list_item * c = topics->first;

    while (c != NULL) {
        subscription * sub = c->data;

        if (sub->session_id == client_session_id) {
            free(sub->topic_pattern);
            free(sub->id);
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

/**
 * @param topic_mask
 * @param matches
 * @return number of matches or -1 on error
 */
int pubsub_find_matching(char* topic_mask, general_list * matches) {

    if (topics->size == 0) return matches->size;
    // NOTE: This function should be harnessed in a higher abstraction level \
not to use wildcards in the topic_mask
    char * topic_mask_wild_card = strchr(topic_mask, '*');
    int topic_mask_len = topic_mask_wild_card == NULL ?
            strlen(topic_mask) :
            topic_mask_wild_card - topic_mask;

    int len2;
    char * wild_card;
    subscription * sub;

    general_list_item * c = topics->first;

    while (c != NULL) {
        sub = c->data;

        wild_card = strchr(sub->topic_pattern, '*');
        len2 = wild_card == NULL ?
                strlen(sub->topic_pattern) :
                wild_card - sub->topic_pattern;

        // if no wild-card and length does not match
        // or there is a wildcard but len is shorter
        // => it is a no match!
        if (wild_card == NULL && len2 != topic_mask_len
                || wild_card != NULL && topic_mask_len < len2
                ) {
            c = c->next;
            continue;
        }

        // MIN of topic_mask_len and len2
        int len = topic_mask_len < len2 ? topic_mask_len : len2;
        if (strncmp(topic_mask, sub->topic_pattern, len) == 0) {
            list_add(matches, sub);
        }

        c = c->next;
    }

    return matches->size;
}

int pubsub_size() {
    return topics->size;
}

void pubsub_to_str(char* dst, size_t dst_size) {
    if (topics->size == 0) return;

    general_list_item * c = topics->first;

    while (c != NULL) {
        subscription * sub = c->data;
        size_t dst_len = strlen(dst);

        if ((strlen(sub->topic_pattern) + dst_len + 2) > dst_size) break;

        if (dst_len > 0) strcat(dst, ",");

        strcat(dst, sub->topic_pattern);

        c = c->next;
    }
}