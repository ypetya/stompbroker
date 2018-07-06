#ifndef STOMP_H
#define STOMP_H

/**
 * # Responsibilities:
 * 
 * - process stomp messages
 * 
*/

struct StompInput
{
    char *message;
    size_t messageLength;
    int clientId;
    int clientFD;
};

void doStomp(struct StompInput *input);

#endif