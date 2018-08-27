#include "parse_args.h"

#include "logger.h"
#include <string.h>
#include <sys/sysinfo.h>

stomp_app_config config;

char buffer[20];

stomp_app_config config_parse_args() {
    snprintf(buffer, sizeof (buffer), "%d", DEFAULT_PORT);
    config.backlog = DEFAULT_BACKLOG;
    config.port = (char*) &buffer;
    config.max_input_queue_size = DEFAULT_INPUT_QUEUE_LIMIT;
    config.input_buffer_size = DEFAULT_INPUT_BUFFER_SIZE;
    config.processors = get_nprocs();
    
    info("server: This system has %d processors configured and "
        "%d processors available.\n",
        get_nprocs_conf(), get_nprocs());
    info("server: Port:%d \n", DEFAULT_PORT);
    info("server: Maximum message size: %d\n", DEFAULT_INPUT_BUFFER_SIZE);
    info("server: Input queue limit: %d\n", DEFAULT_INPUT_QUEUE_LIMIT);
    
    return config;
}

stomp_app_config* config_get_config() {
    return &config;
}