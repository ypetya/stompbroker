
#include "../logger.h"

#include <fcntl.h>
#include <errno.h>
// For "select" api
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../lib/constants.h"
#include "../../lib/thread_safe_queue.h"
#include "../../lib/clone_str.h"
#include "process.h"

#include <string.h> // memcpy

#define BUFFER_SIZE 1000
char readBuffer[BUFFER_SIZE];

void *getInAddr(struct sockaddr *sa);

// TODO: change to sys/epoll
void acceptIncomingDataLoop(int listenSockFD) {
    int i;
    int newConnectionFD, clientConnectionFD;
    fd_set readset, allset;
    int clients[FD_SETSIZE],
            maxClientID = -1;
    for (i = 0; i < FD_SETSIZE; i++) {
        clients[i] = -1;
    }

    FD_ZERO(&allset);
    FD_SET(listenSockFD, &allset);

    info("server: waiting for connections...\n");

    struct sockaddr_in their_addr; // connector's address information
    socklen_t sin_size = sizeof their_addr;
    char * clientAddressStr; //[INET6_ADDRSTRLEN];
    int maxClientFD = listenSockFD;

    ts_queue queue;
    ts_queue_init(&queue);
    start_process_threads(&queue);

    while (YES) {
        fflush(stdout);
        readset = allset;
        int numOfFDsChanged = select(maxClientFD + 1, &readset, NULL, NULL, NULL); // Blocking
        // New connections
        if (FD_ISSET(listenSockFD, &readset)) {
            //listenSockFD has changes
            if ((newConnectionFD = accept(listenSockFD,
                    (struct sockaddr *) &their_addr,
                    &sin_size)) == -1)
                trace("server: Accept failed: %s\n", strerror(errno))
            else {
                clientAddressStr = inet_ntoa(their_addr.sin_addr);
                info("server: got connection from %s\n", clientAddressStr);
                for (i = 0; i < FD_SETSIZE; i++) {
                    if (clients[i] < 0) {
                        clients[i] = newConnectionFD;
                        info("server: clients[%d]=%d;\n", i, newConnectionFD);
                        break;
                    }
                }

                fcntl(newConnectionFD, F_SETFL, O_NONBLOCK | O_NDELAY);

                if (maxClientFD < newConnectionFD) maxClientFD = newConnectionFD;
                if (i > maxClientID) maxClientID = i;

                FD_SET(newConnectionFD, &allset);
                info("server: MaxClientID=%d;\n", maxClientID);

                if (--numOfFDsChanged <= 0) {
                    continue;
                }
            }
        }

        // handle client-sockets
        for (i = 0; i <= maxClientID; i++) {
            clientConnectionFD = clients[i];
            if (clientConnectionFD < 0) continue;
            if (FD_ISSET(clientConnectionFD, &readset)) {
                int received_length = recv(clientConnectionFD, readBuffer, sizeof (readBuffer), 0);
                if (received_length < 1) {
                    if (received_length == 0) {
                        info("server: socket closed nicely. client[%d]\n", i);
                    } else {
                        info("server: socked closed with error: %s\n", strerror(errno))
                    }
                    clients[i] = -1;
                    FD_CLR(clientConnectionFD, &allset);
                    close(clientConnectionFD);
                } else {

                    message * incoming_message = message_create(
                            clientConnectionFD,
                            readBuffer,
                            received_length);
                    
                    ts_enqueue(&queue, incoming_message);
                }
                if (--numOfFDsChanged <= 0) break;
            }
        } // end client-sockets

        // experiment with this
        usleep(10);
    }
}

// get sockaddr, IPv4 or IPv6:

void *getInAddr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *) sa)->sin_addr);

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}
