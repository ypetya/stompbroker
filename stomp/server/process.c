#include <pthread.h>
#include <unistd.h>

#include "process.h"
#include "../logger.h"
#include "../lib/thread_safe_queue.h"
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
ts_queue input_queue;
ts_queue output_queue;

ts_queue * process_start_threads() {

    stomp_start();

    ts_queue_init(&input_queue);
    ts_queue_init(&output_queue);

    worker_data.input_q = &input_queue;
    worker_data.output_q = &output_queue;

    pthread_create(&reader_thread_id, NULL, reader_thread, &worker_data);
    pthread_create(&writer_thread_id, NULL, writer_thread, &output_queue);

    return &input_queue;
}

void process_kill_threads() {
    stomp_stop(&output_queue);

    ts_enqueue(&output_queue, message_poison_pill());
    ts_enqueue(&input_queue, message_poison_pill());

    pthread_join(reader_thread_id, NULL);
    pthread_join(writer_thread_id, NULL);

    ts_queue_free(&input_queue);
    ts_queue_free(&output_queue);
}

void *writer_thread(void *vargp) {
    ts_queue * output_queue = vargp;

    int res = 0;
    while (YES) {

        res = -1;
        message * msg = (message*) ts_dequeue(output_queue);

        if (msg != NULL) {
            if (msg->fd == -1) {
                debug(" * Writer thread: Poison pill detected.\n");
                message_destroy(msg);
                break;
            }
            debug(">>>\n%s\n", msg->content);
            // len+1 is required! STOMP standard requires to send 
            // a closing zero octet
            res = send(msg->fd, msg->content, strlen(msg->content)+1, 0);
            if (res < 0) {
                perror("Could not send message. Client may disconnected");
            }
            message_destroy(msg);
        }

        usleep(10);
    }

    //pthread_exit((void *) EXIT_SUCCESS);

    return NULL;
}

void *reader_thread(void *vargp) {
    worker_thread_data_struct * queues = vargp;

    ts_queue * input_queue = queues->input_q;
    ts_queue * output_queue = queues->output_q;

    while (YES) {

        message * msg = ts_dequeue(input_queue);

        if (msg != NULL) {
            if (msg->fd == -1) {
                debug(" * Reader thread: Poison pill detected.\n");
                message_destroy(msg);
                break;
            }
            debug("<<<\n%s\n", msg->content);
            stomp_process(output_queue, msg);
            message_destroy(msg);
        }

        usleep(10);
    }

    //pthread_exit((void *) EXIT_SUCCESS);

    return NULL;
}
