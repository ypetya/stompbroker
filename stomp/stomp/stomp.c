#include "stomp.h"
#include "../logger.h"
#include <pthread.h>
#include <sys/socket.h>

// FIXME: allow parallel writes to different fileDescriptors
pthread_mutex_t outputLock = PTHREAD_MUTEX_INITIALIZER;

void doStomp(int clientFD, char *input, int inputLen) {
    input[inputLen]='\0';
    //print("Stomp received: %s",input);
    //print("Constructing output");

    pthread_mutex_lock(&outputLock);

    const char * ANS = "CONNECTED\nversion:1.2\n\n";

    if (send(clientFD, ANS, strlen(ANS), 0) == -1){
        perror("send");
    }

    pthread_mutex_unlock(&outputLock);
}