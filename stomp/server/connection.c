#include "connection.h"
#include "../logger.h"

#include "../stomp/stomp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1000
// #define cyclic_inc(x,arr) (x=++x % sizeof arr)
// #define init_arr(arr,zero) memset(arr, zero, sizeof arr);

void *connectionWorkerThread(void *vargp)
{
    struct connectionInfo *cinf = (struct connectionInfo *)vargp;
    int clientConnectionFD = cinf->fd; 
    char readBuffer[BUFFER_SIZE];

    debug("New read thread for FD: %d\n", cinf->fd);

    int n=recv(clientConnectionFD,readBuffer,sizeof(readBuffer),0);
    if (n < 1){
        if(n==0) {
            info("server: socket closed nicely. client[%d]\n", cinf->connectionIndex);
        } else {
            info("server: socked closed with error: %s\n", strerror(errno))
        }
        close(clientConnectionFD);
        (*cinf->clientSlot) = -1;
        FD_CLR(clientConnectionFD, cinf->fdSet);
    }
    else {
        doStomp(cinf->fd,(char*)&readBuffer,n);
    }

    free(cinf);

    return NULL;
}
