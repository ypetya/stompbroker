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
 * A '*' matches any rest of the destination, including an empty one:
 * pattern "/topic/" + '*' matches destination "/topic/" too. Live delivery always worked this way;
 * stored TTL messages now use the same rule.
 */
int pubsub_topic_matches(const char* pattern, const char* destination) {
    const char * wild_card = strchr(pattern, '*');
    if (wild_card == NULL) return strcmp(pattern, destination) == 0;
    size_t prefix_len = (size_t) (wild_card - pattern);
    return strlen(destination) >= prefix_len && strncmp(pattern, destination, prefix_len) == 0;
}

/**
 * Collects the subscriptions matching a destination.
 *
 * @param topic_mask destination of the message (wildcards are rejected before)
 * @param matches linked list pointer where the matches are collected to
 * @return number of matches
 */
int pubsub_find_matching(char* topic_mask, general_list * matches) {
    for (general_list_item * c = topics->first; c != NULL; c = c->next) {
        subscription * sub = c->data;
        if (pubsub_topic_matches(sub->topic_pattern, topic_mask)) list_add(matches, sub);
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