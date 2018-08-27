/**
 * String list implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emalloc.h"
#include "string_list.h"
#include "clone_str.h"

string_list_item* create_string_list_item(char *src);

void string_list_add(string_list* str_list, char* new_str) {

    string_list_item * list = str_list->list;
    string_list_item * new_elem = create_string_list_item(new_str);

    if (list == NULL) {
        str_list->list = new_elem;
        return;
    }

    string_list_item* cur = list;
    while (cur->next != NULL) cur = cur->next;
    cur->next = new_elem;
}

void string_list_print(string_list* list) {
    int i = 0;
    string_list_item* c = list->list;

    while (c != NULL) {
        printf("%d. %s\n", i++, c->word);
        c = c->next;
    }
}

void string_list_free_items(string_list* list) {
    string_list_item* c = list->list;
    string_list_item* n;

    while (c != NULL) {
        n = c->next;
        // NOTE: this
        free(c->word);
        free(c);
        c = n;
    }
}

string_list_item* create_string_list_item(char *src) {
    string_list_item* ptr = emalloc(sizeof (string_list_item));
    ptr->word = clone_str(src);
    return ptr;
}