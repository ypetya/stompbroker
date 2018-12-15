
#include <stdlib.h>
#include "../../lib/emalloc.h"
#include "../../lib/general_list.h"
#include "../stomp.h"
#include "../../lib/logger.h"
#include "session_storage.h"

int* clients;
int number_of_clients;

int session_storage_add_new(int external_id) {
    if (number_of_clients >= MAX_NUMBER_OF_CONNECTIONS) return -2;
    int session_id = external_id % MAX_NUMBER_OF_CONNECTIONS;
    if(clients[session_id]>0) return -1;
    clients[session_id]=external_id;
    
    number_of_clients++;

    return session_id;
}

void session_storage_init() {    
    number_of_clients=0;
    clients = emalloc(sizeof (int) * (MAX_NUMBER_OF_CONNECTIONS+1));
}

void session_storage_dispose(ts_queue* q_out) {
    for (int i=0;i<number_of_clients;i++) {
        int external_id = clients[i];
        if(external_id>0){
            message * disconnect = message_disconnect(external_id);
            ts_enqueue(q_out, disconnect);
        }
    }

    free(clients);

    clients = NULL;
}

// This is very important to keep this fast!
int session_storage_fetch_client_id(int external_id) {
    int client_id = external_id % MAX_NUMBER_OF_CONNECTIONS;
    if(clients[client_id]==external_id) return client_id;
    return -1;
}

// This is very important to keep this fast!
int session_storage_fetch_external_id(int session_id) {
    return clients[session_id];
}

void session_storage_remove(int session_id) {
    number_of_clients--;
    clients[session_id]=0;
}

int session_storage_size() {
    return number_of_clients;
}