#include "stomp.h"
#include "../logger.h"
#include <sys/socket.h>

#include "../../lib/thread_safe_queue.h"
#include "parser.h"

void doStomp(ts_queue* output_queue, message *input) {
    
    parsed_message * pm = parse_message(input);
    
    switch(pm->command) {
        default: {
            message * err = message_error(input->fd, "Invalid message!\n\0");
        
            ts_enqueue(output_queue, err);
        }
    }
    
    free_parsed_message(pm);
}