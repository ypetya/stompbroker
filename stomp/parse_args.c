#include "parse_args.h"

#include "logger.h"
#include <string.h>

stomp_app_config config;

char buffer[20];

stomp_app_config config_parse_args() {
    snprintf(buffer, sizeof (buffer), "%d", DEFAULT_PORT);
    config.backlog = DEFAULT_BACKLOG;
    config.port = (char*) &buffer;
    config.max_input_queue_size = DEFAULT_INPUT_QUEUE_LIMIT;
    config.input_buffer_size = DEFAULT_INPUT_BUFFER_SIZE;
    
    info("server: Using default configuration. port:%d \n", DEFAULT_PORT);
    info("server: Maximum message size: %d\n", DEFAULT_INPUT_BUFFER_SIZE);
    info("server: Input queue limit: %d\n", DEFAULT_INPUT_QUEUE_LIMIT);
    
    return config;
}

stomp_app_config* config_get_config() {
    return &config;
}