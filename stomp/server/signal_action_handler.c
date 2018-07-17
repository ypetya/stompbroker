
#include <errno.h> // errno
#include <stdio.h>  // NULL
#include <stdlib.h> // exit
#include <sys/wait.h>
#include <signal.h>
#include "signal_action_handler.h"
#include "../stomp/stomp.h"

void sigchldHandler(int s);
void exitOnSignal() {
    struct sigaction signalActionStruct;
    signalActionStruct.sa_handler = sigchldHandler; // reap all dead processes
    sigemptyset(&signalActionStruct.sa_mask);
    signalActionStruct.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &signalActionStruct, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }
}

void sigchldHandler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    // waitpid waits childprocesses to end
    while (waitpid(-1, NULL, WNOHANG) > 0);

    stomp_stop();

    errno = saved_errno;
}