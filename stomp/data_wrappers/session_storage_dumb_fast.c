/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include <stdlib.h>
#include "../../lib/emalloc.h"
#include "../../lib/general_list.h"
#include "../stomp.h"
#include "../../lib/logger.h"
#include "session_storage.h"

int session_id;

general_list * clients;

// Instead of this linked list, some memory mapping / bitmask could work
int session_storage_add_new(int external_id) {
    if (clients == NULL) return -1;

    session_item * s;
    for (general_list_item * client = clients->first;
            client != NULL;
            client = client->next) {
        s = client->data;
        if (s->external_id == external_id) return -1;
    }

    s = emalloc(sizeof (session_item));
    s->external_id = external_id;
    s->session_id = external_id;
    
    session_id++;

    list_add(clients, s);
    //debug(" * Session new session %d for fd %d\n", s->session_id, s->external_id);

    return external_id;
}

void session_storage_init() {
    session_id = 0;
    clients = emalloc(sizeof (general_list));
}

void session_storage_dispose(ts_queue* q_out) {
    for (general_list_item* client = clients->first;
            client != NULL;
            client = client->next) {
        session_item * s = client->data;
        message * disconnect = message_disconnect(s->external_id);
        ts_enqueue(q_out, disconnect);
    }

    list_free(clients);

    clients = NULL;
}

int session_storage_fetch_client_id(int external_id) {
    return external_id;
}
// This is very important to keep this fast!
// O(1) solution:
int session_storage_fetch_external_id(int session_id) {
    return session_id;
}

void session_storage_remove(int session_id) {
    //debug(" * Session delete by session_id %d\n", session_id);

    int i = 0;
    session_item * s;
    for (general_list_item * client = clients->first;
            client != NULL;
            client = client->next, i++) {
        s = client->data;
        if (s->session_id == session_id) {
            break;
        }
    }

    s = list_unchain_at(clients, i);

    free(s);
}

int session_storage_size() {
    return clients->size;
}