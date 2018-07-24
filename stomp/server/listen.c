#include "listen.h"
#include "../logger.h"
#include "process.h"

#include <stdlib.h> // exit()
#include <unistd.h> // close(fd)

// For address and connection
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h> // listen
#include <arpa/inet.h>

// starting the reader thread
#include <pthread.h>

#include "accept.h"
#include "signal_action_handler.h"
#include "../lib/constants.h"


int do_listen(char *port, int backlog)
{
    int listenSockFD = findPortAndStartListening(port, backlog);
    exitOnSignal();
    accept_incoming_data_loop(listenSockFD);

    return 0;
}

int findPortAndStartListening(char *port, int backlog) {
    int listenSockFD;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        warn("getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((listenSockFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(listenSockFD, SOL_SOCKET, SO_REUSEADDR, &YES, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(listenSockFD, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listenSockFD);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)
    {
        warn("server: failed to bind\n");
        exit(1);
    }

    if (listen(listenSockFD, backlog) == -1)
    {
        perror("listen");
        exit(1);
    }
    
    return listenSockFD;
}
