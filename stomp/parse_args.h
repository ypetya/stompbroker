#ifndef PARSEARGS_H
#define PARSEARGS_H

/**
 * # Responsiblity:
 * 
 * - Provide config values
*/

#define DEFAULT_PORT "3490"  // the port users will be connecting to
#define DEFAULT_BACKLOG 10     // how many pending connections queue will hold

struct CONFIG_STRUCTURE {
    char * port;
    int backlog;
};

typedef struct CONFIG_STRUCTURE ConfigStruct;

ConfigStruct parseArgs();

#endif