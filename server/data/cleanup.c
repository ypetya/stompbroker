
#include "../data/session_storage.h"
#include "../../stomp/data_wrappers/pub_sub.h"
#include "../../websocket/buffer.h"

#include <unistd.h>
#include <pthread.h>

void clean_by_fd(int fd) {
    if(session_storage_get(fd)){
        close(fd);

        session_storage_remove(fd);
        buffer_item *bi = ws_buffer_find(fd);
        ws_buffer_free(bi);
    }
}