/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include <stdlib.h>
#include "../../lib/emalloc.h"
#include "../../lib/general_list.h"
#include "../stomp.h"
#include "../../lib//logger.h"

general_list * clients;

int session_storage_add_new(int external_id) {
    if (clients == NULL) return -1;
    for (general_list_item * client = clients->first;
            client != NULL;
            client = client->next) {
        if (*(int*) client->data == external_id) return -1;
    }

    int * data = emalloc(sizeof (int));
    memcpy(data, &external_id, sizeof (int));

    list_add(clients, data);
    debug(" * Session new index %d for fd %d\n", clients->size - 1, external_id);
    return clients->size - 1; //:) - single thread funky
}

void session_storage_init() {
    clients = emalloc(sizeof (general_list));
}

void session_storage_dispose(ts_queue* q_out) {
    for (general_list_item* client = clients->first;
            client != NULL;
            client = client->next) {
        message * disconnect = message_disconnect(*((int *) client->data));
        ts_enqueue(q_out, disconnect);
    }

    list_free(clients);

    clients = NULL;
}

// external_id is the file descriptor of the socket

int session_storage_find_client_id(int external_id) {
    debug(" * Session find by external_id:%d\n", external_id);
    int ret = list_index_of(clients, &external_id, sizeof(int));
    debug(" -> index: %d\n", ret);
    return ret;
}

void session_storage_remove(int index) {
    debug(" * Session delete by index %d\n", index);
    list_remove_at(clients, index);
    //general_list_item * item = list_unchain_at(clients, index);
    //free(item->data)
}

int session_storage_size() {
    return clients->size;
}