#include "parse_args.h"

#include "logger.h"
#include <string.h>
#include <sys/sysinfo.h>

stomp_app_config config;

char buffer[20];

void config_parse_parse(int argc, char* argv[]);

stomp_app_config config_parse_args(int argc, char* argv[]) {
    config.port = NULL;
    config.processors = 0;
    config.max_input_queue_size = 0;

    config_parse_parse(argc, argv);

    if (config.port == NULL) {
        snprintf(buffer, sizeof (buffer), "%d", DEFAULT_PORT);
        config.port = (char*) &buffer;
    }

    config.backlog = DEFAULT_BACKLOG;
    if (config.max_input_queue_size == 0) {
        config.max_input_queue_size = DEFAULT_INPUT_QUEUE_LIMIT;
    }
    config.input_buffer_size = DEFAULT_INPUT_BUFFER_SIZE;
    if (config.processors == 0) config.processors = get_nprocs();

    info("server: This system has %d processors configured and "
            "%d processors available.\n"
            "server: processors:%d\n",
            get_nprocs_conf(), get_nprocs(), config.processors);
    info("server: port:%s \n", config.port);
    info("server: Maximum message size: %d\n", config.input_buffer_size);
    info("server: max_input_queue_size: %d\n", config.max_input_queue_size);

    return config;
}

stomp_app_config* config_get_config() {
    return &config;
}

void config_parse_parse(int argc, char* argv[]) {
    for (int i = 0; i < argc; i++) {
        char * token = strtok(argv[i], "=");
        if (strncmp("processors", token, 10) == 0) {
            token = strtok(NULL, "=");
            int p;
            sscanf(token, "%d", &p);
            config.processors = p;
        } else if (strncmp("port", token, 4) == 0) {
            token = strtok(NULL, "=");
            strcpy(buffer, token);
            config.port = buffer;
        } else if (strncmp("max_input_queue_size", token, 20) == 0) {
            token = strtok(NULL, "=");
            sscanf(token, "%d", &config.max_input_queue_size);
        }
    }
}