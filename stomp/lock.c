


// FIXME: allow parallel writes to different fileDescriptors
#include <pthread.h>

pthread_mutex_t locks[FD_SETSIZE];

void init_locks() {
    for(int i=0;i<FD_SETSIZE;i++) pthread_mutex_init(&locks[i],NULL);
}

void lock(int clientId){
    pthread_mutex_lock(&locks[clientId]);
}
void unlock(int clientId){
    pthread_mutex_unlock(&locks[clientId]);
}