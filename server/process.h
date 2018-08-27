#ifndef PROCESS_CONNECTION_THREAD_H
#define PROCESS_CONNECTION_THREAD_H

#include "../lib/thread_safe_queue.h"

ts_queue* process_start_threads();

void process_kill_threads();

#endif