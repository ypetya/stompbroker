

//#define DEBUG 1
//#define DEBUG_OUTPUT

// pick-up all the implementations

#include "lib/emalloc.c"
#include "lib/general_list.c"
#include "lib/queue.c"
#include "lib/thread_safe_queue.c"
#include "lib/clone_str.c"
#include "lib/associative_array.c"

#include "parse_args.c"


#include "websocket/main.h"
#include "stomp/main.h"
#include "server/main.h"
/**
 * # Responsibility
 * 
 * Fire-up standalone Server:
 * 1. Read config
 * 2. Start listening
*/
int main(int argc, char* argv[])
{
    stomp_app_config config = config_parse_args(argc,argv);
    
    return do_listen(config.port, config.backlog);
}
