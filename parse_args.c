#include "parse_args.h"

#include "logger.h"
#include <string.h>

stomp_app_config config_main;

char buffer[20];

void config_parse_parse(int argc, char* argv[argc]);

stomp_app_config config_parse_args(int argc, char* argv[argc]) {
    config_main.port = NULL;
    config_main.processors = 0;
    config_main.max_input_queue_size = 0;
    config_main.max_stale_queue_size = 0;
    config_main.ttl=0;

    config_parse_parse(argc, argv);

    if (config_main.port == NULL) {
        snprintf(buffer, sizeof (buffer), "%d", DEFAULT_PORT);
        config_main.port = (char*) &buffer;
    }

    config_main.backlog = DEFAULT_BACKLOG;

    if (config_main.max_input_queue_size == 0)
        config_main.max_input_queue_size = DEFAULT_INPUT_QUEUE_LIMIT;
    
    if (config_main.max_stale_queue_size == 0)
        config_main.max_stale_queue_size = DEFAULT_INPUT_QUEUE_LIMIT;

    config_main.input_buffer_size = DEFAULT_INPUT_BUFFER_SIZE;
    config_main.output_buffer_size = DEFAULT_OUTPUT_BUFFER_SIZE;
    if (config_main.processors == 0) config_main.processors = get_nprocs();

    info("server: processors:%d\n", config_main.processors);
    info("server: port:%s \n", config_main.port);
    info("server: Maximum message size: %d\n", config_main.input_buffer_size);
    info("server: max_input_queue_size: %d max_stale_queue_size: %d\n", 
        config_main.max_input_queue_size, config_main.max_stale_queue_size);
    info("server: TTL: %d micro-seconds\n", config_main.ttl);

    return config_main;
}

stomp_app_config* config_get_config() {
    return &config_main;
}

void config_parse_parse(int argc, char* argv[]) {
    for (int i = 0; i < argc; i++) {
        char * token = strtok(argv[i], "=");
        if (strncmp("processors", token, 10) == 0) {
            token = strtok(NULL, "=");
            sscanf(token, "%d", &config_main.processors);
        } else if (strncmp("port", token, 4) == 0) {
            token = strtok(NULL, "=");
            strcpy(buffer, token);
            config_main.port = buffer;
        } else if (strncmp("max_input_queue_size", token, 20) == 0) {
            token = strtok(NULL, "=");
            sscanf(token, "%d", &config_main.max_input_queue_size);
        } else if (strncmp("max_stale_queue_size", token, 20) == 0) {
            token = strtok(NULL, "=");
            sscanf(token, "%d", &config_main.max_stale_queue_size);
        } else if (strncmp("TTL", token, 3)==0) {
            token = strtok(NULL, "=");
            sscanf(token, "%d", &config_main.ttl);
            // convert millies to micros
            config_main.ttl *= 1000;
        }
    }
}