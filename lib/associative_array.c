#include "associative_array.h"

#include <stdio.h>
#include "emalloc.h"
#include "clone_str.h"

aa_item * aa_item_create(char* key, char* value);

int aa_set_or_put_item(aa_item** p, aa_item* a, char * key, char * value);

void aa_put(associative_array* aa, char * key, char * value) {
    if (aa->root == NULL) {
        aa->root = aa_item_create(key, value);
    } else {
        aa_set_or_put_item(&aa->root, aa->root, key, value);
    }
}

void aa_rotate_right_left(aa_item** parents_pointer, aa_item* node);
void aa_rotate_left_right(aa_item** parents_pointer, aa_item* node);
void aa_rotate_right(aa_item** parents_pointer, aa_item* node);
void aa_rotate_left(aa_item** parents_pointer, aa_item* node);

int max_h(aa_item* node) {
    int ret = 0;
    if (node->left && node->left->height) ret = node->left->height;
    if (node->right && node->right->height > ret) ret = node->right->height;
    return ret;
}

int balance(aa_item* node) {
    int left = 0,right = 0;
    if (node->left && node->left->height) left = node->left->height;
    if (node->right && node->right->height) right = node->right->height;
    return left - right;
}

/**
 * 
 * @param a
 * @param key
 * @param value
 * @return TRUE if new item created 
 */
int aa_set_or_put_item(aa_item** p, aa_item* a, char * key, char * value) {
    int inserted_new = 0;
    int c = strcmp(key, a->key);

    if (c == 0) {
        char * newVal = clone_str(value);
        free(a->value);
        a->value = newVal;
    } else if (c < 0) {
        if (a->left == NULL) {
            a->left = aa_item_create(key, value);
            inserted_new = 1;
        } else {
            inserted_new = aa_set_or_put_item(&a->left, a->left, key, value);
        }
    } else if (c > 0) {
        if (a->right == NULL) {
            a->right = aa_item_create(key, value);
            inserted_new = 1;
        } else {
            inserted_new = aa_set_or_put_item(&a->right, a->right, key, value);
        }
    }

    if (inserted_new) {
        a->height = max_h(a) + 1;

        int b = balance(a);
        if (b<-1) {
            if (balance(a->left) <= 0) {
                aa_rotate_right(p, a);
            } else {
                aa_rotate_right_left(p, a);
            }
        } else if (b > 1) {
            if (balance(a->right) >= 0) {
                aa_rotate_left(p, a);
            } else {
                aa_rotate_left_right(p, a);
            }
        }

    }

    return inserted_new;
}

/*
 * Fundamental
 * 
 * gp -> p -> c
 * 
 * Rotations to keep the balance:
 * LL, RR, LR, RL 
 * 
 * Normal rotations
 * ----------------
 * 
 * Left:
 * set temp= gp->r
 * set gp->r = temp->l
 * set temp->l = gp
 * use temp instead of gp
 * 
 * Right:
 * set temp= gp->l
 * set gp->l = temp->r
 * set temp->r = gp
 * use temp instead of gp
 * 
 * Double rotations
 * ----------------
 * 
 * to keep tree in order
 * 
 * 4         4  -> L     6
 *  \         \         / \
 *   8 -> R    6       4   8
 *  /           \
 * 6             8
 * 
 */

/**
 *   parent ( weight <= -2)
 *    /
 *   l1
 *   /
 * @param parent
 * @param node
 */
void aa_rotate_right(aa_item** parents_pointer, aa_item* node) {
    aa_item* tmp = node->left;
    node->left = tmp->right;
    tmp->right = node;
    *parents_pointer = tmp;
    node->height-=1;
    tmp->height+=1;
}

void aa_rotate_left(aa_item** parents_pointer, aa_item* node) {
    aa_item* tmp = node->right;
    node->right = tmp->left;
    tmp->left = node;
    *parents_pointer = tmp;
    node->height-=1;
    tmp->height+=1;
}

void aa_rotate_right_left(aa_item** parents_pointer, aa_item* node) {
    aa_rotate_right(&node->right, node->right);
    aa_rotate_left(parents_pointer, node);
}

void aa_rotate_left_right(aa_item** parents_pointer, aa_item* node) {
    aa_rotate_left(&node->left, node->left);
    aa_rotate_right(parents_pointer, node);
}

int aa_has(aa_item* aa, char * key) {
    if (aa == NULL) return 0;

    int c = strcmp(key, aa->key);

    if (c == 0) {
        return 1;
    } else if (c < 0) {
        return aa_has(aa->left, key) + 1;
    }
    return aa_has(aa->right, key) + 1;
}

aa_item* aa_get(aa_item* aa, char * key) {
    if (aa == NULL) return NULL;

    int c = strcmp(key, aa->key);

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
    ptr->height = 0;

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
    if (aa != NULL) {
        aa_item_free(aa->root);
        free(aa);
    }
}

associative_array * aa_create() {
    return emalloc(sizeof (associative_array));
}
