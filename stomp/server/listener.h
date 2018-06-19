#ifndef LISTENER_H
#define LISTENER_H

/**
 * # Responsibilities:
 * 
 * - listen on a specific port
 * - handle connections
 * - maintain available connections array
 * - provide available connections for read
 * 
*/

int doListen(char *port, int backlog);

#endif