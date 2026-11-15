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

void pubsub_subscribe(char* topic_str, int client_session_id, char* subscription_id, unsigned int generation) {
    if (topics == NULL) return;
    subscription * new_sub = emalloc(sizeof (subscription));
    new_sub->session_id = client_session_id;
    new_sub->topic_pattern = clone_str(topic_str);
    new_sub->id = clone_str(subscription_id);
    new_sub->generation = generation;

    list_add(topics, new_sub);
}

typedef int (*subscription_filter)(const subscription * sub, int client_session_id, const char * id);

static int of_client(const subscription * sub, int client_session_id, const char * id) {
    return sub->session_id == client_session_id;
}

static int of_client_with_id(const subscription * sub, int client_session_id, const char * id) {
    return sub->session_id == client_session_id && strcmp(sub->id, id) == 0;
}

/** Unchains and frees every subscription matching the filter. @return number removed */
static int remove_subscriptions(subscription_filter matches, int client_session_id, const char * id) {
    int removed = 0;
    general_list_item * p = NULL;
    general_list_item * c = topics->first;

    while (c != NULL) {
        subscription * sub = c->data;
        general_list_item * next = c->next;

        if (matches(sub, client_session_id, id)) {
            free(sub->topic_pattern);
            free(sub->id);
            free(sub);
            if (p == NULL) topics->first = next;
            else p->next = next;
            if (topics->last == c) topics->last = p;
            free(c);
            topics->size--;
            removed++;
        } else {
            p = c;
        }
        c = next;
    }
    return removed;
}

int pubsub_unsubscribe(int client_session_id, const char* subscription_id) {
    return remove_subscriptions(of_client_with_id, client_session_id, subscription_id);
}

void pubsub_remove_client(int client_session_id) {
    remove_subscriptions(of_client, client_session_id, NULL);
}

int pubsub_has_subscription(int client_session_id, const char* subscription_id) {
    for (general_list_item * c = topics->first; c != NULL; c = c->next)
        if (of_client_with_id(c->data, client_session_id, subscription_id)) return 1;
    return 0;
}

/**
 * 
 * The method collects out matching subscriptions to a given topic_mask pattern. 
 * 
 * @param topic_mask the message is sent to
 * @param matches linked list pointer where the matches are collected to
 * @return number of matches or -1 on error
 */
int pubsub_find_matching(char* topic_mask, general_list * matches) {

    if (topics->size == 0) return matches->size;
    /* NOTE: This function should be harnessed in a higher abstraction level not
     *  to use wildcards in the topic_mask */
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
        if ((wild_card == NULL && len2 != topic_mask_len) ||
                (wild_card != NULL && topic_mask_len < len2)
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