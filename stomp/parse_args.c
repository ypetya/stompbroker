#include "parse_args.h"

#include "logger.h"
#include <string.h>

ConfigStruct config;

char buffer[20];
    
ConfigStruct parseArgs()
{
    snprintf(buffer,sizeof(buffer),"%d",DEFAULT_PORT);
    config.backlog = DEFAULT_BACKLOG;
    config.port = (char*)&buffer;

    info("server: Using default configuration. port:%d \n", DEFAULT_PORT);

    return config;
}