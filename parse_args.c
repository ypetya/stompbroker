#include "parse_args.h"

#include "logger.h"
#include "lib/sysinfo_compat.h"
#include <string.h>

static stomp_app_config config_main;

static char buffer[20] = {0};

void config_parse_parse(int argc, char* argv[]);

void config_init() {
    config_main.port = NULL;
    config_main.processors = 0;
    config_main.max_input_queue_size = 0;
    config_main.max_stale_queue_size = 0;
    config_main.max_stale_queue_bytes = 0;
    config_main.ttl = 0;
    config_main.ws_allowed_origins = NULL;
    config_main.ws_allowed_hosts = NULL;
    config_main.write_timeout_ms = 0;
    config_main.max_frame_size = 0;
}

void config_setup_missing_defaults() {
    if (config_main.port == NULL || *config_main.port == 0) {
        snprintf(buffer, sizeof (buffer), "%d", DEFAULT_PORT);
        config_main.port = (char*) &buffer;
    }

    config_main.backlog = DEFAULT_BACKLOG;

    if (config_main.max_input_queue_size == 0)
        config_main.max_input_queue_size = DEFAULT_INPUT_QUEUE_LIMIT;

    if (config_main.max_stale_queue_size == 0)
        config_main.max_stale_queue_size = DEFAULT_INPUT_QUEUE_LIMIT;

    if (config_main.max_stale_queue_bytes == 0)
        config_main.max_stale_queue_bytes = DEFAULT_STALE_QUEUE_BYTES;

    config_main.input_buffer_size = DEFAULT_INPUT_BUFFER_SIZE;
    config_main.output_buffer_size = DEFAULT_OUTPUT_BUFFER_SIZE;
    if (config_main.max_frame_size == 0) config_main.max_frame_size = DEFAULT_MAX_FRAME_SIZE;
    if (config_main.processors == 0) config_main.processors = get_nprocs();
    if (config_main.write_timeout_ms <= 0) config_main.write_timeout_ms = DEFAULT_WRITE_TIMEOUT_MS;

    info("server: processors:%d\n", config_main.processors);
    info("server: port:%s \n", config_main.port);
    info("server: read buffer size: %d\n", config_main.input_buffer_size);
    info("server: Maximum frame size: %u\n", config_main.max_frame_size);
    info("server: max_input_queue_size: %d max_stale_queue_size: %d max_stale_queue_bytes: %lu\n",
            config_main.max_input_queue_size, config_main.max_stale_queue_size, config_main.max_stale_queue_bytes);
    info("server: TTL: %d micro-seconds\n", config_main.ttl);
    info("server: write_timeout: %d ms\n", config_main.write_timeout_ms);
    info("server: ws_allowed_origins: %s ws_allowed_hosts: %s\n",
            config_main.ws_allowed_origins ? config_main.ws_allowed_origins : "(any)",
            config_main.ws_allowed_hosts ? config_main.ws_allowed_hosts : "(any)");
}

stomp_app_config config_setup_by_args(int processors,
        int port,
        int max_input_queue_size,
        int max_stale_queue_size,
        int ttl
        ) {
    config_init();

    config_main.processors = processors;
    
    if(port)
        sprintf(buffer, "%d", port);
    config_main.port = buffer;
    config_main.max_input_queue_size = max_input_queue_size;
    config_main.max_stale_queue_size = max_stale_queue_size;
    config_main.ttl = ttl * 1000;
    
    config_setup_missing_defaults();

    return config_main;
}

stomp_app_config config_parse_args(int argc, char* argv[]) {
    config_init();
    config_parse_parse(argc, argv);
    config_setup_missing_defaults();

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
        } else if (strncmp("max_stale_queue_bytes", token, 21) == 0) {
            token = strtok(NULL, "=");
            if (token) sscanf(token, "%lu", &config_main.max_stale_queue_bytes);
        } else if (strncmp("max_stale_queue_size", token, 20) == 0) {
            token = strtok(NULL, "=");
            sscanf(token, "%d", &config_main.max_stale_queue_size);
        } else if (strncmp("ws_allowed_origins", token, 18) == 0) {
            config_main.ws_allowed_origins = strtok(NULL, "");
        } else if (strncmp("ws_allowed_hosts", token, 16) == 0) {
            config_main.ws_allowed_hosts = strtok(NULL, "");
        } else if (strncmp("max_frame_size", token, 14) == 0) {
            token = strtok(NULL, "=");
            if (token) sscanf(token, "%u", &config_main.max_frame_size);
        } else if (strncmp("write_timeout", token, 13) == 0) {
            token = strtok(NULL, "=");
            if (token) sscanf(token, "%d", &config_main.write_timeout_ms);
        } else if (strncmp("TTL", token, 3) == 0) {
            token = strtok(NULL, "=");
            sscanf(token, "%d", &config_main.ttl);
            // convert millies to micros
            config_main.ttl *= 1000;
        }
    }
}