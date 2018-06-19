#ifndef CONNECTION_H
#define CONNECTION_H

/**
 * Responsibility:
 * 
 * - to read from a connection - pass to stomp logic, start a new thread
 * - to write to a connection picked up from stomp logic running on other thread(s?)
 * 
*/

#include <sys/select.h>

struct connectionInfo {
    int * clientSlot;
    int fd;
    int connectionIndex;
    fd_set * fdSet;
};

void *connectionWorkerThread(void *vargp);


#endif