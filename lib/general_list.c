/**
 * String list implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emalloc.h"
#include "general_list.h"

general_list_item* create_general_list_item(void *data);

void list_add(general_list* g_list, void* data) {

    general_list_item * list = g_list->list;
    general_list_item * new_elem = create_general_list_item(data);

    if (list == NULL) {
        g_list->list = g_list->last = new_elem;
        g_list->size += 1;
        return;
    }
    g_list->size += 1;
    g_list->last->next = new_elem;
}

void list_print(general_list* list) {
    int i = 0;
    printf("List size: %d\n", list->size);
    general_list_item* c = list->list;

    while (c != NULL) {
        printf("%d. Data length: %d\n", i++, (int) sizeof (c->data));
        c = c->next;
    }
}

void list_free_items(general_list* list);
void list_free(general_list* list) {
    list_free_items(list);
    free(list);
}

void list_free_items(general_list* list) {
    general_list_item* c = list->list;
    general_list_item* n;

    while (c != NULL) {
        // FIXME: leak?
        n = c->next;

        free(c->data);
        free(c);
        c = n;
    }
}

general_list_item* create_general_list_item(void *data) {
    general_list_item* ptr = emalloc(sizeof (general_list_item));
 
    ptr->data = data;
    ptr->next = NULL;
    
    return ptr;
}

int list_index_of(general_list * list, void* data) {
    int ret_val = -1;
    size_t siz = sizeof (*data);
    size_t siz_c;

    general_list_item* c = list->list;

    if (c != NULL) {

        for (ret_val = 0; c != NULL;
                ret_val++,
                c = c->next) {
            siz_c = sizeof (*c->data);
            if (siz == siz_c &&
                    memcmp(c->data, data, siz) == 0) break;
        }
    }

    return ret_val;

}

int list_remove_at(general_list*list, int ix) {
    if (ix < 0 || ix >= list->size) return -1;

    general_list_item* drop;
    if (ix == 0) {
        drop = list->list;
        list->list = list->list->next;
        free(drop->data);
        free(drop);
        return 0;
    }
    general_list_item* c = list->list;
    general_list_item* p = NULL;
    int i;
    for (i = 0; i < ix; i++, p = c, c = c->next);
    p->next = c->next;
    free(c->data);
    free(c);

    return i;
}