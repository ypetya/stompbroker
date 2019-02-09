
#include <stdlib.h>
#include "../../lib/emalloc.h"
#include "../../lib/general_list.h"
#include "../../logger.h"
#include "session_storage.h"

int* clients;
int number_of_clients;

pthread_mutex_t session_storage_mutex;

void session_storage_init() {
    info("Session storage supports %d connections\n", MAX_NUMBER_OF_CONNECTIONS)
    number_of_clients = 0;
    clients = emalloc(sizeof (int) * (MAX_NUMBER_OF_CONNECTIONS + 1));
    pthread_mutex_init(&session_storage_mutex, (const pthread_mutexattr_t*) PTHREAD_PROCESS_PRIVATE);
}

void session_storage_lock() {
    pthread_mutex_lock(&session_storage_mutex);
}

void session_storage_unlock() {
    pthread_mutex_unlock(&session_storage_mutex);
}

void session_storage_dispose() {
    free(clients);
    number_of_clients = 0;
    clients = NULL;
}

int session_storage_add_new(int fd) {
    if (number_of_clients >= MAX_NUMBER_OF_CONNECTIONS ||
            fd > MAX_NUMBER_OF_CONNECTIONS
            ) return MAX_SESSION_NUMBER_EXCEEDED;
    
    if (clients[fd] > 0) return SESSION_IS_IN_USE;
    clients[fd] = fd;

    number_of_clients++;

    return fd;
}

void session_storage_remove(int client_id) {
    number_of_clients--;
    clients[client_id] = EMPTY_SESSION;
}

int session_storage_get(int client_id) {
    return clients[client_id];
}

int session_storage_is_encoded(int client_id) {
    return clients[client_id] & FD_IS_ENCODED_MASK;
}

void session_storage_set_encoded(int fd) {
    clients[fd] = fd | FD_IS_ENCODED_MASK;
}

int session_storage_encoded_size() {
    int num = 0;
    for (int i = 0; i < MAX_NUMBER_OF_CONNECTIONS; i++) {
        if (clients[i] & FD_IS_ENCODED_MASK) num++;
    }
    return num;
}

int session_without_flags(int fd) {
    return fd & FD_MASK;
}

int session_is_encoded(int fd) {
    return fd & FD_IS_ENCODED_MASK;
}
    
int session_set_encoded(int fd) {
    return fd | FD_IS_ENCODED_MASK;
}

int session_is_cmd_purge(int fd) {
    return fd & FD_CMD_PURGE_SESSION;
}

int session_set_cmd_purge(int fd) {
    return fd | FD_CMD_PURGE_SESSION;
}

int session_wo_cmd_purge(int fd) {
    return fd & ( FD_IS_ENCODED_MASK | FD_MASK); 
}

int session_storage_size() {
    return number_of_clients;
}