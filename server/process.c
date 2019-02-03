#include <pthread.h>

#include "process.h"
#include "../logger.h"
#include "../lib/queue.h"
#include "../lib/thread_safe_queue.h"
#include "../stomp/stomp.h"
#include "../lib/emalloc.h"
#include "../parse_args.h"
#include "data/session_storage.h"
#include "data/message/with_payload_length.h"
#include "data/message/with_timestamp.h"
#include "threads/worker_thread_data.h"
#include "threads/reader_thread.h"
#include "threads/writer_thread.h"

typedef struct workers_thread_st {
    pthread_t reader_thread_id;
    pthread_t * writers;
    int writers_count;
} worker_thread_struct;

// dynamic
// workers is allocated dynamically as it can has multiple writer_thread_ids
worker_thread_struct * workers;

// static
worker_thread_data_struct worker_data;
ts_queue input_queue;
ts_queue output_queue;
queue stale_queue;

ts_queue * process_start_threads() {

    session_storage_init();
    stomp_start();

    ts_queue_init(&input_queue);
    ts_queue_init(&output_queue);
    queue_init(&stale_queue);

    worker_data.input_q = &input_queue;
    worker_data.output_q = &output_queue;
    worker_data.stale_queue = &stale_queue;

    stomp_app_config * config = config_get_config();
    workers = emalloc(sizeof (worker_thread_struct));
    // at least one writer must exist
    // TODO: rename processors to writer_thread_count
    workers->writers_count = config->processors - 2 < 1 ? 1 : config->processors - 2;
    workers->writers = emalloc(sizeof (pthread_t) * workers->writers_count);

    // start multiple workers
    pthread_create(&workers->reader_thread_id, NULL, reader_thread, &worker_data);
    for (int i = 0; i < workers->writers_count; i++)
        pthread_create(&(workers->writers[i]), NULL, writer_thread, &worker_data);


    return &input_queue;
}

void process_kill_threads() {
    // kill reader thread
    ts_enqueue(&input_queue, message_poison_pill_with_timestamp());
    // kill writer threads
    for (int i = 0; i < workers->writers_count; i++)
        ts_enqueue(&output_queue, message_poison_pill_with_frame_len());

    // JOIN threads
    pthread_join(workers->reader_thread_id, NULL);
    for (int i = 0; i < workers->writers_count; i++)
        pthread_join(workers->writers[i], NULL);

    //free workers
    free(workers->writers);
    free(workers);

    ts_queue_free(&input_queue);
    ts_queue_free(&output_queue);
    queue_free(&stale_queue);
}


