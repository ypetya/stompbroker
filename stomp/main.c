
// pick-up all the implementations

#include "../lib/signal_action_handler.c"
#include "../lib/queue.c"
#include "../lib/thread_safe_queue.c"
#include "../lib/clone_str.c"
#include "parse_args.c"
#include "server/listen.c"
#include "server/process.c"
#include "server/accept.c"
#include "stomp/stomp.c"
#include "stomp/parser.c"
#include "stomp/message.c"

/**
 * # Responsibility
 * 
 * Fire-up standalone Server:
 * 1. Read config
 * 2. Start listening
*/
int main(void)
{
    ConfigStruct config = parseArgs();
    
    return doListen(config.port, config.backlog);
}
