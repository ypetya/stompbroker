
#include "../data/session_storage.h"
#include "../../stomp/data_wrappers/pub_sub.h"
#include "../../websocket/buffer.h"

#include <unistd.h>
#include <pthread.h>

pthread_mutex_t cleanup_lock;

void init_cleanup() {
    pthread_mutex_init(&cleanup_lock, (const pthread_mutexattr_t*) PTHREAD_PROCESS_PRIVATE);
}

void clean_by_fd(int fd) {
    pthread_mutex_lock(&cleanup_lock);
    if(session_storage_get(fd)){
        close(fd);

        session_storage_remove(fd);
        buffer_item *bi = ws_buffer_find(fd);
        ws_buffer_free(bi);
    }
    pthread_mutex_unlock(&cleanup_lock);
}