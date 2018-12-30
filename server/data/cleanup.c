
#include "../data/session_storage.h"
#include "../../stomp/data_wrappers/pub_sub.h"
#include "../../websocket/buffer.h"

void clean_by_fd(int fd) {
    int client_id = session_storage_remove_external(fd);
    pubsub_remove_client(client_id);
    buffer_item *bi = ws_buffer_find(fd);
    ws_buffer_free(bi);
}