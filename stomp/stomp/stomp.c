#include "stomp.h"
#include "../logger.h"
#include <sys/socket.h>

#include "../lock.h"

void doStomp(struct StompInput *input) {
    //input[inputLen]='\0';
    //print("Stomp received: %s",input);
    //print("Constructing output");

    lock(input->clientId);
    const char * ANS = "CONNECTED\nversion:1.2\n\n";

    if (send(input->clientFD, ANS, strlen(ANS), 0) == -1){
        perror("send");
    }

    unlock(input->clientId);
}