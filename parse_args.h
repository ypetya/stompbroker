#ifndef PARSEARGS_H
#define PARSEARGS_H

/**
 * # Responsiblity:
 * 
 * - Parse config values - from command line?
 * - Provide config values
 */

// the port users will be connecting to
#define DEFAULT_PORT 3490
// how many pending connections queue will hold
#define DEFAULT_BACKLOG 10
// maximum input queue size
#define DEFAULT_INPUT_QUEUE_LIMIT 1000000
// maximum size of a single message
#define DEFAULT_INPUT_BUFFER_SIZE 4000

#define DEFAULT_OUTPUT_BUFFER_SIZE 10000

struct CONFIG_STRUCTURE {
    char * port;
    int backlog;
    unsigned int max_input_queue_size;
    int input_buffer_size;
    int output_buffer_size; // per thread
    int processors;
};

typedef struct CONFIG_STRUCTURE stomp_app_config;

stomp_app_config* config_get_config();
stomp_app_config config_parse_args(int argc, char* argv[]);

#endif