#ifndef LISTEN_H
#define LISTEN_H

int doListen(char *port, int backlog);
int findPortAndStartListening(char *port, int backlog);

#endif