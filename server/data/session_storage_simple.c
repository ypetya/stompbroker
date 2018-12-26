
#include <stdlib.h>
#include "../../lib/emalloc.h"
#include "../../lib/general_list.h"
#include "../../lib/logger.h"
#include "session_storage.h"

int* clients;
int number_of_clients;

int session_storage_add_new(int external_id) {
    if (number_of_clients >= MAX_NUMBER_OF_CONNECTIONS) return MAX_SESSION_NUMBER_EXCEEDED;
    int session_id = external_id % MAX_NUMBER_OF_CONNECTIONS;
    if (clients[session_id] > 0) return SESSION_IS_IN_USE;
    clients[session_id] = external_id;

    number_of_clients++;

    return session_id;
}

void session_storage_init() {
    info("Session storage supports %d connections\n", MAX_NUMBER_OF_CONNECTIONS)
    number_of_clients = 0;
    clients = emalloc(sizeof (int) * (MAX_NUMBER_OF_CONNECTIONS + 1));
}

void session_storage_dispose() {
    free(clients);

    clients = NULL;
}

// This is very important to keep this fast!

int session_storage_fetch_client_id(int external_id) {
    int client_id = external_id % MAX_NUMBER_OF_CONNECTIONS;
    if (clients[client_id] > 0) return client_id;
    return SESSION_IS_INVALID;
}

// This is very important to keep this fast!

int session_storage_fetch_external_id(int session_id) {
    return clients[session_id] & FD_MASK;
}

int session_is_encoded(int external_id) {
    int client_id = external_id % MAX_NUMBER_OF_CONNECTIONS;

    return clients[client_id] & FD_IS_ENCODED_MASK;
}

void session_set_encoded(int external_id) {
    int client_id = external_id % MAX_NUMBER_OF_CONNECTIONS;
    int with_encoded_flag = clients[client_id] | FD_IS_ENCODED_MASK;
    clients[client_id] = with_encoded_flag;
}

int session_is_connected(int session_id) {
    return clients[session_id] & FD_IS_CONNECTED_MASK;
}

void session_set_connected(int session_id) {
    int external_id = clients[session_id] | FD_IS_CONNECTED_MASK;
    clients[session_id] = external_id;
}

void session_storage_remove(int session_id) {
    number_of_clients--;
    clients[session_id] = EMPTY_SESSION;
}

void session_storage_remove_external(int external_id) {
    int client_id = external_id % MAX_NUMBER_OF_CONNECTIONS;
    number_of_clients--;
    clients[client_id] = EMPTY_SESSION;
}

int session_storage_connected_size() {
    int num = 0;
    for (int i = 0; i < MAX_NUMBER_OF_CONNECTIONS; i++) {
        if (clients[i] & FD_IS_CONNECTED_MASK) num++;
    }
    return num;
}