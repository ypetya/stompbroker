#ifndef LOCK_H
#define LOCK_H

/**
 * # Responsibilities:
 * 
 * - handle MUTEX per client-slot
 * 
*/
void init_locks();
void lock(int clientId);
void unlock(int clientId);

#endif