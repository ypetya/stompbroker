#ifndef PARSEARGS_H
#define PARSEARGS_H

/**
 * - Parse config values from command line
 * - Fill out config with a function call - this is an API for embedding
 * - Provide config values for other modules
 */

// the port users will be connecting to
#define DEFAULT_PORT 3490
// how many pending connections queue will hold
#define DEFAULT_BACKLOG 10
// maximum input/stale queue size
#define DEFAULT_INPUT_QUEUE_LIMIT 1000000
// maximum size of a single message
#define DEFAULT_INPUT_BUFFER_SIZE 16000

#define DEFAULT_OUTPUT_BUFFER_SIZE 10000
// a writer gives up on a connection that accepts no data for this long
#define DEFAULT_WRITE_TIMEOUT_MS 5000

struct CONFIG_STRUCTURE {
    char * port;
    int backlog;

    unsigned int max_input_queue_size;
    unsigned int ttl;
    unsigned int max_stale_queue_size;

    int input_buffer_size;
    int output_buffer_size; // per thread
    int processors; // (writers_count := max(processors-2,1)
    int write_timeout_ms; // slow consumer timeout of the writer threads

    // WebSocket handshake allowlists, comma separated. NULL = allow all (default)
    char * ws_allowed_origins;
    char * ws_allowed_hosts;
};

typedef struct CONFIG_STRUCTURE stomp_app_config;

stomp_app_config* config_get_config();

stomp_app_config config_parse_args(int argc, char* argv[]);
stomp_app_config config_setup_by_args(int processors,
        int port,
        int max_input_queue_size,
        int max_stale_queue_size,
        int ttl
        );

#endif