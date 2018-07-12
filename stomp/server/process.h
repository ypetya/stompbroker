#ifndef PROCESS_CONNECTION_THREAD_H
#define PROCESS_CONNECTION_THREAD_H

#include "../../lib/thread_safe_queue.h"

void start_process_threads(ts_queue * input_queue);

#endif