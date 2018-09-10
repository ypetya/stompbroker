#include "associative_array.h"

#include <stdio.h>
#include "emalloc.h"
#include "clone_str.h"

aa_item * aa_item_create(char* key, char* value);

void aa_set_or_put_item(aa_item* a, char * key, char * value);

void aa_put(associative_array* aa, char * key, char * value) {
    if (aa->root == NULL) {
        aa->root = aa_item_create(key, value);
    } else {
        aa_set_or_put_item(aa->root, key, value);
    }
}

void aa_set_or_put_item(aa_item* a, char * key, char * value) {
    int c = strcmp(a->key, key);

    if (c == 0) {
        char * newVal = clone_str(value);
        free(a->value);
        a->value = newVal;
    } else if (c < 0) {
        if (a->left == NULL) {
            a->left = aa_item_create(key, value);
        } else {
            aa_set_or_put_item(a->left, key, value);
        }
    } else if (c > 0) {
        if (a->right == NULL) {
            a->right = aa_item_create(key, value);
        } else {
            aa_set_or_put_item(a->right, key, value);
        }
    }
}

int aa_has(aa_item* aa, char * key) {
    if (aa == NULL) return -1;

    int c = strcmp(aa->key, key);

    if (c == 0) {
        return 1;
    } else if (c < 0) {
        return aa_has(aa->left, key);
    }
    return aa_has(aa->right, key);
}

aa_item* aa_get(aa_item* aa, char * key) {
    if (aa == NULL) return NULL;

    int c = strcmp(aa->key, key);

    if (c == 0) {
        return aa;
    } else if (c < 0) {
        return aa_get(aa->left, key);
    }
    return aa_get(aa->right, key);
}

char * aa_get_str_buf(aa_item*aa, char* buffer);

// get a string representation for the subtree

char* aa_create_str_representation(aa_item* aa) {
    if (aa == NULL) return emalloc(1);
    return aa_get_str_buf(aa, NULL);
}

// size of <key>:<value>

size_t aa_item_get_str_size(aa_item*aa) {
    return strlen(aa->key) + strlen(aa->value) + 1;
}

char * aa_get_str_buf(aa_item*aa, char* buffer) {
    int current_len = buffer == NULL ? 0 : strlen(buffer);
    int append_len = aa_item_get_str_size(aa);

    // append new_buffer
    char * new_buffer = NULL;

    if (buffer == NULL) {
        new_buffer = emalloc(append_len + 1);
        snprintf(new_buffer, append_len + 1, "%s:%s", aa->key, aa->value);
    } else if (append_len > 0) {
        // \n \0
        new_buffer = emalloc(current_len + 1 + append_len + 1);

        strncpy(new_buffer, buffer, current_len);
        new_buffer[current_len] = '\n';
        snprintf(new_buffer + current_len + 1, append_len + 1, "%s:%s", aa->key, aa->value);
        free(buffer);
    }

    if (aa->left != NULL) new_buffer = aa_get_str_buf(aa->left, new_buffer);
    if (aa->right != NULL) new_buffer = aa_get_str_buf(aa->right, new_buffer);

    return new_buffer;
}

aa_item * aa_item_create(char* key, char* value) {
    aa_item * ptr = emalloc(sizeof (aa_item));

    ptr->key = clone_str(key);
    ptr->value = clone_str(value);
    ptr->left = ptr->right = NULL;

    return ptr;
}

void aa_merge(associative_array* target, aa_item* tree) {

    aa_put(target, tree->key, tree->value);

    if (tree->left) aa_merge(target, tree->left);
    if (tree->right) aa_merge(target, tree->right);
}

void aa_item_free(aa_item * ptr) {
    if (ptr == NULL) return;
    free(ptr->key);
    free(ptr->value);
    aa_item_free(ptr->left);
    aa_item_free(ptr->right);
    free(ptr);
}

void aa_free(associative_array * aa) {
    if (aa != NULL){
        aa_item_free(aa->root);
        free(aa);
    }
}
