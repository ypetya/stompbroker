#include "parse_args.h"

#include "logger.h"

ConfigStruct config;

ConfigStruct parseArgs()
{
    config.backlog = DEFAULT_BACKLOG;
    config.port = DEFAULT_PORT;

    info("server: Using default configuration. port:%s \n", DEFAULT_PORT);

    return config;
}