#ifndef LISTEN_H
#define LISTEN_H

int do_listen(char *port, int backlog);
int findPortAndStartListening(char *port, int backlog);

#endif