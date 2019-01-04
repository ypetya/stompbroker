

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

void* dequeue(queue * q) {
    general_list_item * head = q->first;
    if (head == NULL) return NULL;
    void * data = head->data;
    q->first = head->next;
    if (q->next == head) {
        q->next = q->first;
    }
    free(head);
    q->size--;

    return data;
}

void* peek_next(queue * q) {
    general_list_item * current = q->next;

    if (current != NULL) {
        general_list_item * next = current->next;

        q->next = (next == NULL) ? q->first : next;
    }
    return current->data;
}

void dequeue_item(queue * q, void * item) {
    general_list_item* current = q->first;
    general_list_item* last = NULL;

    while (current != NULL && current->data != item) {
        last = current;
        current = current->next;
    }
    if (current == NULL) return; // No match
    
    q->size--;

    if (last == NULL) { //first item
        q->first = current->next;
        q->next = q->first;
    } else {

        if (current->next == NULL) { //last item
            q->last = last;
        }
        last->next = current->next;
        q->next = last->next;
    }
    free(current);
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
    q->first = q->last = q->next = NULL;
    q->size = 0;
}