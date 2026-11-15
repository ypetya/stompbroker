
#include "../data/session_storage.h"
#include "../../stomp/data_wrappers/pub_sub.h"

#include <unistd.h>
#include <pthread.h>

void clean_by_fd(int fd) {
    if(session_storage_get(fd)){
        close(fd);

        session_storage_remove(fd);
        // WebSocket buffers/state are owned by the epoll thread: it resets them
        // in its own close path and whenever the fd number is accepted again.
    }
}