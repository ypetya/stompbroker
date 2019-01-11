#include <unistd.h>
#include "writer_thread.h"
#include "data/string_message.h"
#include "../logger.h"

// #define DEBUG_OUTPUT

void *writer_thread(void *vargp) {
    ts_queue * output_queue = vargp;

    int res = 0;
    while (YES) {

        res = -1;
        message_with_frame_len * msg = (message_with_frame_len*) ts_dequeue(output_queue);

        if (msg != NULL) {
            if (msg->fd == -1) {
                debug(" * Writer thread: Poison pill detected.\n");
                message_wl_destroy(msg);
                break;
            }
            debug(">>>\n%s\n", msg->content);
            size_t len = ws_output_filter(msg);

            res = write(msg->fd, msg->content, len);
            
            if (res <= 0) {
                perror("Could not send message. Client may disconnected");
                warn("fd:%d\n", msg->fd);
            }
            #ifdef DEBUG_OUTPUT
            printf("Wrote: fd: %d, len: %d, wrote: %d, data:",msg->fd, len, res);
            for(int i=0;i<len;i++) printf("%02x",msg->content[i] & 0xff);
            printf("\n\n");
            #endif
            
            message_wl_destroy(msg);
        }

        usleep(10);
    }

    return NULL;
}