
/* 
 * File:   pub_sub.h
 * Author: peter
 *
 * Created on July 23, 2018, 3:37 PM
 */

#ifndef PUB_SUB_H
#define PUB_SUB_H

#ifdef __cplusplus
extern "C" {
#endif

    /** SUBSCRIBE ack header: auto (default), client (cumulative), client-individual. */
    typedef enum {
        ACK_AUTO = 0,
        ACK_CLIENT,
        ACK_CLIENT_INDIVIDUAL
    } ack_mode;

    typedef struct subscription_st {
        int session_id;
        char * topic_pattern;
        char * id;
        unsigned int generation; // session generation of the subscriber (see session_storage_generation)
        ack_mode ack;
        // worker queues ('#'), ack != auto: most messages delivered but not acknowledged yet
        unsigned int prefetch;
        unsigned int in_flight; // worker queue messages delivered, not acknowledged yet
        unsigned long long last_pick; // worker queues: when it was chosen last (0: never), see pick_worker
    } subscription;

    void pubsub_init();
    void pubsub_dispose();

    /** @return the new subscription (auto ack); the caller may set ack and prefetch */
    subscription * pubsub_subscribe(char* topic_str, int client_session_id, char* subscription, unsigned int generation);
    /** Removes the subscription of the client with the given id (STOMP 1.2: id only). @return number removed */
    int pubsub_unsubscribe(int client_session_id, const char* subscription);
    /** @return 1 if the client already has a subscription with this id */
    int pubsub_has_subscription(int client_session_id, const char* subscription);

    void pubsub_remove_client(int client_session_id);

#include "../../lib/general_list.h"
    int pubsub_find_matching(char* topic_mask, general_list * matches);

    /**
     * Does a destination match a subscription pattern? A '*' in the pattern
     * matches any rest of the destination (including nothing).
     */
    int pubsub_topic_matches(const char* pattern, const char* destination);

    /** @return 1 if any subscription matches the destination */
    int pubsub_has_matching(const char* destination);

    int pubsub_size();

    // For diagnostic only
    void pubsub_to_str(char* dst, size_t dst_size);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_H */

