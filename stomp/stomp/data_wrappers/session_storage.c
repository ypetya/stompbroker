/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include <stdlib.h>
#include "../../../lib/emalloc.h"
#include "../../../lib/general_list.h"
#include "../stomp.h"

general_list * clients;


int session_storage_add_new(int client_fd) {
    for(general_list_item * client = clients->list;
            client != NULL;
            client = client->next) {
        if(*(int*)client->data == client_fd) return -1;
    }
    
    int * data = emalloc(sizeof(int));
    memcpy(data,&client_fd,sizeof(int));
    
    list_add(clients, data);
    
    return clients->size - 1; //:) - single thread funky
}


void session_storage_init() {
    clients = emalloc(sizeof (general_list));
}


void session_storage_dispose(ts_queue* q) {
    for(general_list_item* client = clients->list;
            client != NULL;
            client = client->next) {
        // FIXME: leak
        message * disconnect = message_disconnect(*((int *)client->data));
        ts_enqueue(q, disconnect );
    }
    // FIXME: leak
    list_free_items(clients);
}

int session_storage_find_client_id(int client_id){
    return list_index_of(clients, &client_id);
}

void session_storage_remove(int client_id) {
    list_remove_at(clients, client_id);
}