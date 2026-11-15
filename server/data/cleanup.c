
#include "cleanup.h"
#include "session_storage.h"
#include "message/with_timestamp.h"
#include "../threads/output_queue.h"
#include "../../websocket/buffer.h"
#include "../../stomp/framing.h"

#include <unistd.h>
#include <sys/socket.h>

void connection_close(int fd, ts_queue * input_queue, ts_queue * output_queue) {
    shutdown(fd, SHUT_RDWR);
    output_queue_claim_fd(output_queue, fd);

    session_storage_lock();
    int fd_with_flags = session_storage_get(fd);
    if (fd_with_flags != 0) {
        // never dropped: the purge removes the subscriptions of the connection
        message_with_timestamp * purge = message_create_with_timestamp(session_set_cmd_purge(fd_with_flags), "CMD", 0);
        purge->generation = session_storage_generation(fd);
        ts_enqueue(input_queue, purge);
        session_storage_remove(fd);
    }
    session_storage_unlock();

    close(fd);
    *output_queue_stall_of(fd) = (output_stall) {0};
    output_queue_release_fd(output_queue, fd);

    ws_conn_reset(fd);
    stomp_input_reset(stomp_input_of(fd));
}
