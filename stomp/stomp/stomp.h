#ifndef STOMP_H
#define STOMP_H

/**
 * # Responsibilities:
 * 
 * - process stomp messages
 * 
*/

void doStomp(int clientFD, char *input, int inputLen);

#endif