#include <stdlib.h>
#include <string.h>
#include "pending_acks.h"
#include "../../lib/queue.h"
#include "../../lib/emalloc.h"

/* Delivery order == ack id order == sent_at order: new entries go to the tail. */
static queue pending;
static unsigned long long last_ack_id = 0;

void pending_init(void) {
    queue_init(&pending);
}

void pending_dispose(void) {
    pending_ack * entry;
    while ((entry = dequeue(&pending)) != NULL) {
        free_parsed_message(entry->pm);
        free(entry);
    }
}

unsigned long long pending_add(subscription * sub, parsed_message * pm, long long now) {
    pending_ack * entry = emalloc(sizeof (pending_ack));
    entry->ack_id = ++last_ack_id;
    entry->sub = sub;
    entry->session_id = sub->session_id;
    entry->pm = pm;
    entry->sent_at = now;
    enqueue(&pending, entry);
    sub->in_flight++;
    return entry->ack_id;
}

typedef int (*pending_filter)(const pending_ack * entry, const void * ctx);

/** Unchains the entries matching the filter, in order. */
static general_list * take_where(pending_filter matches, const void * ctx) {
    general_list * taken = list_new();
    general_list_item * parent = NULL;
    general_list_item * item = pending.first;
    while (item != NULL) {
        general_list_item * next = item->next;
        pending_ack * entry = item->data;
        if (matches(entry, ctx)) {
            unchain_child(&pending, parent, item);
            free(item);
            entry->sub->in_flight--;
            list_add(taken, entry);
        } else {
            parent = item;
        }
        item = next;
    }
    return taken;
}

static const pending_ack * find(int session_id, unsigned long long ack_id) {
    for (general_list_item * item = pending.first; item != NULL; item = item->next) {
        const pending_ack * entry = item->data;
        if (entry->ack_id == ack_id) return entry->session_id == session_id ? entry : NULL;
    }
    return NULL;
}

static int settled_by(const pending_ack * entry, const void * ctx) {
    const pending_ack * acked = ctx;
    if (entry->sub->ack == ACK_CLIENT)
        return entry->sub == acked->sub && entry->ack_id <= acked->ack_id;
    return entry == acked;
}

general_list * pending_take_acked(int session_id, unsigned long long ack_id) {
    const pending_ack * acked = find(session_id, ack_id);
    return acked != NULL ? take_where(settled_by, acked) : list_new();
}

typedef struct subscriber_st {
    int session_id;
    const char * sub_id; // NULL: any
} subscriber;

static int of_subscriber(const pending_ack * entry, const void * ctx) {
    const subscriber * s = ctx;
    return entry->session_id == s->session_id && (s->sub_id == NULL || strcmp(entry->sub->id, s->sub_id) == 0);
}

general_list * pending_take_of(int session_id, const char * sub_id) {
    subscriber s = {session_id, sub_id};
    return take_where(of_subscriber, &s);
}

general_list * pending_take_sent_before(long long sent_before) {
    // entries are in sent_at order: only a prefix can match, stop at the first newer one
    general_list * taken = list_new();
    while (pending.first != NULL && ((pending_ack *) pending.first->data)->sent_at <= sent_before) {
        pending_ack * entry = dequeue(&pending);
        entry->sub->in_flight--;
        list_add(taken, entry);
    }
    return taken;
}

int pending_size(void) {
    return pending.size;
}
