
#include "parse_args.c"
#include "server/listener.c"
#include "server/connection.c"
#include "stomp/stomp.c"

/**
 * # Responsibility
 * 
 * Fire-up standalone Server:
 * 1. Read configs
 * 2. Start listening
*/
int main(void)
{
    ConfigStruct config = parseArgs();
    
    return doListen(config.port, config.backlog);
}
