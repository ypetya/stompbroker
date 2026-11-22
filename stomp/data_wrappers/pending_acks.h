#ifndef PENDING_ACKS_H
#define PENDING_ACKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pub_sub.h"
#include "../parser.h"
#include "../../lib/general_list.h"

    /**
     * Worker queue ('#') messages delivered to a subscription with ack:client or
     * ack:client-individual and not acknowledged yet. The store owns pm until the
     * entry is taken out; the subscription's in_flight counts its entries.
     */
    typedef struct pending_ack_st {
        unsigned long long ack_id; // value of the MESSAGE ack header, increasing
        subscription * sub; // valid until the subscription is removed
        int session_id;
        parsed_message * pm;
        long long sent_at; // message_timestamp_now() of the delivery
    } pending_ack;

    void pending_init(void);
    /** Frees every entry and its message. */
    void pending_dispose(void);

    /** Stores pm as delivered to sub. @return the ack id of the delivery */
    unsigned long long pending_add(subscription * sub, parsed_message * pm, long long now);

    /*
     * The pending_take_* functions unchain the entries and return them in
     * delivery order (never NULL); the caller frees the list and the entries
     * and takes over their messages. The subscriptions' in_flight is decreased.
     */

    /**
     * Entries an ACK/NACK with ack_id from session_id settles: the entry itself
     * (client-individual) or it and every older entry of its subscription
     * (client). Empty if session_id has no such entry.
     */
    general_list * pending_take_acked(int session_id, unsigned long long ack_id);
    /** Entries of the session's subscription sub_id, or of all its subscriptions if sub_id is NULL. */
    general_list * pending_take_of(int session_id, const char * sub_id);
    /** Entries delivered at or before sent_before. */
    general_list * pending_take_sent_before(long long sent_before);

    int pending_size(void);

#ifdef __cplusplus
}
#endif

#endif /* PENDING_ACKS_H */
