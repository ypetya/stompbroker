

#include <stdio.h>
#include <stdlib.h>

#include "emalloc.h"
#include "queue.h"
#include "general_list.h"

void enqueue(queue * q, void *new_data) {
    general_list_item * new_list_item = create_general_list_item(new_data);

    if (q->size == 0) {
        q->last = q->first = new_list_item;
    } else {
        q->last->next = new_list_item;
        q->last = new_list_item;
    }
    q->size++;
}


int enqueue_limited(queue * q, void *new_data, unsigned int limit) {
    int ret_val = 0;

    if (q->size < limit)
        enqueue(&q, new_data);
    else
        ret_val = -1;

    return ret_val;
}

void* dequeue(queue * q) {
    general_list_item * head = q->first;
    if(head==NULL) return NULL;
    void * data = head->data;
    q->first = head->next;
    if(q->last==head) q->last=NULL;
    free(head);
    q->size--;

    return data;
}

/**
 * @return next list item
*/
void unchain_child(queue * q, general_list_item * parent, general_list_item * child) {
    q->size--;

    // was first item
    if(parent==NULL) q->first=child->next;
    else parent->next = child->next;
    // was last item
    if(q->last==child) q->last=parent;
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