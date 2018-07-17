#include <pthread.h>
#include <unistd.h>

#include "process.h"
#include "../logger.h"
#include "../../lib/thread_safe_queue.h"
#include "../stomp/stomp.h"

typedef struct worker_thread_data_st {
    ts_queue * input_q;
    ts_queue * output_q;
} worker_thread_data_struct;

// start thread   
pthread_t reader_thread_id, writer_thread_id;

void *writer_thread(void *vargp);
void *reader_thread(void *vargp);


worker_thread_data_struct worker_data;
ts_queue output_queue;

void start_process_threads(ts_queue * input_queue) {
    
    stomp_start();
    
    ts_queue_init(&output_queue);
    
    worker_data.input_q = input_queue;
    worker_data.output_q = &output_queue;

    pthread_create(&reader_thread_id, NULL, reader_thread, &worker_data);
    pthread_create(&writer_thread_id, NULL, writer_thread, &output_queue);
}

void *writer_thread(void *vargp) {
    ts_queue * output_queue = (ts_queue*) vargp;
    
    int res = 0;
    while (YES) {

        message * msg = (message*)ts_dequeue(output_queue);
        
        if (msg != NULL) {
            // unsafe: info("output queue size: %d\n", output_queue->q.size);
            res = send(msg->fd, msg->content, strlen(msg->content), 0);
            if (res<0) {
                perror("could not send message.");
            }
            message_destroy(msg);
        }

        usleep(10);
    }

    return NULL;
}

void *reader_thread(void *vargp) {
    worker_thread_data_struct * queues = (worker_thread_data_struct *) vargp;
    
    ts_queue * input_queue = queues->input_q;
    ts_queue * output_queue = queues->output_q;

    while (YES) {

        message * msg = (message*)ts_dequeue(input_queue);
        
        if (msg != NULL) {
            //unsafe: info("input queue size: %d", output_queue->q.size);
            stomp_process(output_queue, msg);
            message_destroy(msg);
        }

        usleep(10);
    }

    return NULL;
}
