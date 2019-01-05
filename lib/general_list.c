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

    general_list_item * new_elem = create_general_list_item(data);

    if (g_list->first == NULL) {
        g_list->first = g_list->last = new_elem;
        g_list->size = 1;
        return;
    }
    g_list->size = g_list->size + 1;
    g_list->last->next = new_elem;
    g_list->last = new_elem;
}

void list_print(general_list* list) {
    int i = 0;
    printf("List size: %d\n", list->size);
    general_list_item* c = list->first;

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

general_list * list_new() {
    general_list * list = emalloc(sizeof (general_list));
    return list;
}

/**
 * Free items, but do not free data!
 */
void list_clear(general_list* list) {
    general_list_item* c = list->first;
    general_list_item* n;

    while (c != NULL) {
        n = c->next;
        free(c);
        c = n;
    }
}

void list_free_items(general_list* list) {
    general_list_item* c = list->first;
    general_list_item* n;

    while (c != NULL) {
        n = c->next;

        // NOTE: leak when data contains a struct, which has not been freed
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

int list_index_of(general_list * list, void* data, size_t siz) {
    int ret_val = -1;

    general_list_item* c = list->first;

    if (c != NULL) {

        ret_val = 0;
        while (c != NULL) {
            if (memcmp(c->data, data, siz) == 0) break;
            ret_val++;
            c = c->next;
        }
    }

    return ret_val;

}

general_list_item* list_get_at(general_list*list, int ix) {
    if (ix < 0 || ix >= list->size) return NULL;

    general_list_item* c = list->first;

    int i = 0;
    while (i < ix) {
        i++;
        c = c->next;
    }
    
    return c;
}

void* list_unchain_at(general_list*list, int ix) {
    if (ix < 0 || ix >= list->size) return NULL;

    general_list_item* c = list->first;
    general_list_item* p = NULL;

    int i = 0;
    while (i < ix) {
        i++;
        p = c;
        c = c->next;
    }
    //unchain
    if (p == NULL) // first item!
        list->first = c->next;
    else
        p->next = c->next;
    // last item!
    if (list->last == c) list->last = p;

    void *data = c->data;
    free(c);
    list->size = list->size - 1;
    return data;
}

int list_remove_at(general_list*list, int ix) {
    void * data = list_unchain_at(list, ix);
    if (data != NULL) {
        free(data);
        return ix;
    }
    return -1;
}