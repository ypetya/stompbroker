#ifndef PROCESS_CONNECTION_THREAD_H
#define PROCESS_CONNECTION_THREAD_H

#include "../lib/thread_safe_queue.h"

typedef struct worker_thread_data_st {
    ts_queue * input_q;
    ts_queue * output_q;
} worker_thread_data_struct;

ts_queue* process_start_threads();

void process_kill_threads();

#endif