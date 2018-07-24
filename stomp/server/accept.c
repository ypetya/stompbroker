
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
char read_buffer[BUFFER_SIZE];

void *get_inbound_address(struct sockaddr *sa);

// TODO: change to sys/epoll
void accept_incoming_data_loop(int listenSockFD) {
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

    ts_queue * input_queue = process_start_threads();

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
                        debug("server: clients[%d]=%d;\n", i, newConnectionFD);
                        break;
                    }
                }

                fcntl(newConnectionFD, F_SETFL, O_NONBLOCK | O_NDELAY);

                if (maxClientFD < newConnectionFD) maxClientFD = newConnectionFD;
                if (i > maxClientID) maxClientID = i;

                FD_SET(newConnectionFD, &allset);
                debug("server: MaxClientID=%d;\n", maxClientID);

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
                int received_length = recv(clientConnectionFD, read_buffer, sizeof (read_buffer), 0);
                if (received_length < 1) {
                    if (received_length == 0) {
                        info("server: socket closed nicely. client[%d]\n", i);
                    } else {
                        info("server: socked closed with error: %s\n", strerror(errno))
                    }
                    clients[i] = -1;
                    FD_CLR(clientConnectionFD, &allset);
                    close(clientConnectionFD);
                    ts_enqueue(input_queue, message_disconnect(clientConnectionFD));
                } else {
                    read_buffer[received_length] = '\0';
                    message * incoming_message = message_create(
                            clientConnectionFD,
                            read_buffer);
                    
                    ts_enqueue(input_queue, incoming_message);
                }
                if (--numOfFDsChanged <= 0) break;
            }
        } // end client-sockets

        // experiment with this
        usleep(10);
    }
}

// get sockaddr, IPv4 or IPv6:

void *get_inbound_address(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *) sa)->sin_addr);

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}
