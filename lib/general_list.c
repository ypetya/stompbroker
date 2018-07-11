/**
 * String list implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emalloc.h"
#include "general_list.h"

general_list_item* create_general_list_item(void *data);

void list_add(general_list* str_list, void* data) {

    general_list_item * list = str_list->list;
    general_list_item * new_elem = create_general_list_item(data);

    if (list == NULL) {
        str_list->list = new_elem;
        return;
    }

    general_list_item* cur = list;
    while (cur->next != NULL) cur = cur->next;
    cur->next = new_elem;
}

void list_print(general_list* list) {
    int i = 0;
    general_list_item* c = list->list;

    while (c != NULL) {
        printf("%d. Data length: %d\n", i++, (int)sizeof(c->data));
        c = c->next;
    }
}

void list_free_items(general_list* list) {
    general_list_item* c = list->list;
    general_list_item* n;

    while (c != NULL) {
        n = c->next;
        
        free(c->data);
        free(c);
        c = n;
    }
}

general_list_item* create_general_list_item(void *data) {
    general_list_item* ptr = emalloc(sizeof (general_list_item));
    ptr->data = data;
    return ptr;
}