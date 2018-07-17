

#include <stdio.h>
#include <stdlib.h>

#include "emalloc.h"
#include "queue.h"
#include "general_list.h"

void enqueue(queue * q, void *new_data) {
    // FIXME: pass in size information?
    general_list_item * new_list_item = create_general_list_item(new_data);

    if (q->size == 0) {
        q->last = q->first = new_list_item;
    } else {
        q->last->next = new_list_item;
        q->last = new_list_item;
    }
    q->size++;
}

void* dequeue(queue * q) {
    general_list_item * head = q->first;
    if(head==NULL) return NULL;
    void * data = head->data;
    q->first = head->next;
    free(head);
    q->size--;

    return data;
}


// NOTE: caution! - frees up the data!
void queue_free(queue *q) {
    general_list_item* c = q->first;
    general_list_item* n;

    while (c != NULL) {
        n = c->next;
        free(c->data);
        free(c);
        c = n;
    }
}

void queue_init(queue *q) {
    q->first = q->last = NULL;
    q->size = 0;
}