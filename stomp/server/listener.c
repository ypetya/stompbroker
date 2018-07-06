#include "listener.h"
#include "../logger.h"
#include "connection.h"

#include <fcntl.h>
#include <errno.h>
// For "select" api
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// For address and connection
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// starting the reader thread
#include <pthread.h>
#include "../lock.h"

const int YES = 1;

void exitOnSignal();
void findPortAndStartListening(int* listenSockFD, char *port, int backlog);
void acceptIncomingDataLoop();

int doListen(char *port, int backlog)
{
    int listenSockFD;
    findPortAndStartListening(&listenSockFD, port, backlog);
    exitOnSignal();
    acceptIncomingDataLoop(listenSockFD);

    return 0;
}

void *getInAddr(struct sockaddr *sa);
void acceptIncomingDataLoop(int listenSockFD) {
    int i;
    int newConnectionFD, clientConnectionFD;
    fd_set readset;
    int clients[FD_SETSIZE],
        maxClientID=-1;
     for(i=0;i<FD_SETSIZE;i++){
        clients[i]=-1;
    }

    FD_ZERO(&readset);
   
    init_locks();

    info("server: waiting for connections...\n");

    struct sockaddr_in their_addr; // connector's address information
    socklen_t sin_size = sizeof their_addr;
    char clientAddressStr[INET6_ADDRSTRLEN];
    int maxClientFD=listenSockFD;

    while (YES) {
        fflush(stdout);
        FD_SET(listenSockFD, &readset);
        int numOfFDsChanged = select(maxClientFD+1, &readset, NULL, NULL, NULL ); // Blocking
        // New connections
        if(FD_ISSET(listenSockFD, &readset)) {
            //listenSockFD has changes
            if ((newConnectionFD = accept(listenSockFD,
                (struct sockaddr *)&their_addr,
                &sin_size)) == -1)
                    trace("server: Accept failed: %s\n", strerror(errno))
            else {
                info("server: got connection from %s\n", (char *)inet_ntoa(their_addr.sin_addr));
                for (i=0;i<FD_SETSIZE;i++){
                    if(clients[i]<0){
                        clients[i]=newConnectionFD;
                        info("clients[%d]=%d;\n", i,newConnectionFD);
                        break;
                    }
                }

                fcntl(newConnectionFD, F_SETFL, O_NONBLOCK);

                if (maxClientFD < newConnectionFD) maxClientFD = newConnectionFD;
                if(i > maxClientID ) maxClientID=i;

                FD_SET(newConnectionFD, &readset);
                info("MaxClientID=%d;\n",maxClientID);
                //printf("new_fd= %d\n",new_fd);
                if (--numOfFDsChanged <= 0){
                    continue;
                }
            }
        }
    
        // handle client-sockets
        for (i=0;i<=maxClientID;i++){
            clientConnectionFD=clients[i];
            if(clientConnectionFD<0) continue;
            if(FD_ISSET(clientConnectionFD, &readset)){
                pthread_t thread_id;
                struct connectionInfo *cinf= malloc(sizeof(struct connectionInfo));
                cinf->clientSlot = &clients[i];
                cinf->fd = clientConnectionFD;
                cinf->connectionIndex = i;
                cinf->fdSet = &readset;
                pthread_create(&thread_id, NULL, connectionWorkerThread, cinf);

                if (--numOfFDsChanged <= 0) break;
            }
        } // end client-sockets
        usleep(1000000);
    }
}

void findPortAndStartListening(int* listenSockFD, char *port, int backlog) {
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
        if ((*listenSockFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(*listenSockFD, SOL_SOCKET, SO_REUSEADDR, &YES, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(*listenSockFD, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(*listenSockFD);
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

    if (listen(*listenSockFD, backlog) == -1)
    {
        perror("listen");
        exit(1);
    }
}


void sigchldHandler(int s);
void exitOnSignal() {
    struct sigaction signalActionStruct;
    signalActionStruct.sa_handler = sigchldHandler; // reap all dead processes
    sigemptyset(&signalActionStruct.sa_mask);
    signalActionStruct.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &signalActionStruct, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }
}

void sigchldHandler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);

    // TODO: close connections

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *getInAddr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *)sa)->sin_addr);

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
