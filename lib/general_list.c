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

    if (g_list->last) {
        g_list->size = g_list->size + 1;
        g_list->last->next = new_elem;
        g_list->last = new_elem;
        return;
    }
    g_list->first = g_list->last = new_elem;
    g_list->size = 1;
}

void list_print(general_list* list) {
    int i = 0;
    printf("List size: %d\n", list->size);
    general_list_item* current_item = list->first;

    while (current_item) {
        printf("%d. Data length: %d\n", i++, (int) sizeof (current_item->data));
        current_item = current_item->next;
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
    general_list_item* current_item = list->first;
    general_list_item* n;

    while (current_item) {
        n = current_item->next;
        free(current_item);
        current_item = n;
    }
}

void list_free_items(general_list* list) {
    general_list_item* current_item = list->first;
    general_list_item* n;

    while (current_item) {
        n = current_item->next;

        // NOTE: leak when data contains a struct, which has not been freed
        free(current_item->data);
        free(current_item);
        current_item = n;
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

    general_list_item* current_item = list->first;

    if (current_item) {

        ret_val = 0;
        while (current_item) {
            if (memcmp(current_item->data, data, siz) == 0) break;
            ret_val++;
            current_item = current_item->next;
        }
    }

    return ret_val;

}

general_list_item* list_get_at(general_list*list, int ix) {
    if (ix < 0 || ix >= list->size) return NULL;

    general_list_item* current_item = list->first;

    int i = 0;
    while (i < ix) {
        i++;
        current_item = current_item->next;
    }

    return current_item;
}

void* list_unchain_at(general_list*list, int ix) {
    if (ix < 0 || ix >= list->size) return NULL;

    general_list_item* current_item = list->first;
    general_list_item* p = NULL;

    int i = 0;
    while (i < ix) {
        i++;
        p = current_item;
        current_item = current_item->next;
    }
    //unchain
    if (p == NULL) // first item!
        list->first = current_item->next;
    else
        p->next = current_item->next;
    // last item!
    if (list->last == current_item) list->last = p;

    void *data = current_item->data;
    free(current_item);
    list->size = list->size - 1;
    return data;
}

int list_remove_at(general_list*list, int ix) {
    void * data = list_unchain_at(list, ix);
    if (data) {
        free(data);
        return ix;
    }
    return -1;
}