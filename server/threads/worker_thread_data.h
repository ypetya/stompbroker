#ifndef WORKER_THREAD_DATA_H
#define WORKER_THREAD_DATA_H

#include "../../lib/queue.h"
#include "../../lib/thread_safe_queue.h"

typedef struct worker_thread_data_st {
    ts_queue * input_q;
    ts_queue * output_q;
    queue * stale_queue;
} worker_thread_data_struct;

#endif