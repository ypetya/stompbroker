#include <pthread.h>
#include <unistd.h>

#include "process.h"
#include "../logger.h"
#include "../lib/thread_safe_queue.h"
#include "../stomp/stomp.h"
#include "../lib/emalloc.h"
#include "../parse_args.h"
#include "data/string_message.h"
#include "data/session_storage.h"

typedef struct worker_thread_data_st {
    ts_queue * input_q;
    ts_queue * output_q;
} worker_thread_data_struct;

typedef struct workers_thread_st {
    pthread_t reader_thread_id;
    pthread_t * writers;
    int writers_count;
} worker_thread_struct;

// dynamic
// workers is allocated dynamically as it can has multiple writer_thread_ids
worker_thread_struct * workers;

void *writer_thread(void *vargp);
void *reader_thread(void *vargp);

// static
worker_thread_data_struct worker_data;
ts_queue input_queue;
ts_queue output_queue;

ts_queue * process_start_threads() {

    session_storage_init();
    stomp_start();

    ts_queue_init(&input_queue);
    ts_queue_init(&output_queue);

    worker_data.input_q = &input_queue;
    worker_data.output_q = &output_queue;

    stomp_app_config * config = config_get_config();
    workers = emalloc(sizeof (worker_thread_struct));
    // atleast one
    workers->writers_count = config->processors - 2 < 1 ? 1 : config->processors - 2;
    workers->writers = emalloc(sizeof (pthread_t) * workers->writers_count);

    // start multiple workers
    pthread_create(&workers->reader_thread_id, NULL, reader_thread, &worker_data);
    for (int i = 0; i < workers->writers_count; i++)
        pthread_create(&(workers->writers[i]), NULL, writer_thread, &output_queue);


    return &input_queue;
}

void process_kill_threads() {
    ts_enqueue(&input_queue, message_poison_pill());
    for (int i = 0; i < workers->writers_count; i++)
        ts_enqueue(&output_queue, message_poison_pill());

    // JOIN threads
    pthread_join(workers->reader_thread_id, NULL);
    for (int i = 0; i < workers->writers_count; i++)
        pthread_join(workers->writers[i], NULL);

    //free workers
    free(workers->writers);
    free(workers);

    ts_queue_free(&input_queue);
    ts_queue_free(&output_queue);
}

// #define DEBUG_OUTPUT

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
            size_t len = ws_output_filter(msg);

            res = write(msg->fd, msg->content, len);
            
            if (res <= 0) {
                perror("Could not send message. Client may disconnected");
                warn("fd:%d\n", msg->fd);
            }
            #ifdef DEBUG_OUTPUT
            printf("Wrote: fd: %d, len: %d, wrote: %d, data:",msg->fd, len, res);
            for(int i=0;i<len;i++) printf("%02x",msg->content[i] & 0xff);
            printf("\n\n");
            #endif
            
            message_destroy(msg);
        }

        usleep(10);
    }

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
                stomp_stop();
                break;
            }

            if (ws_input_filter_handshake(output_queue, msg) == WS_NO_NEED_OF_HANDSHAKE) {
                debug("<<<\n%s\n", msg->content);

                stomp_process(output_queue, msg);
            }
            message_destroy(msg);
        }

        usleep(10);
    }

    return NULL;
}
