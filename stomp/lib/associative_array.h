/*
 * Associative array:
 * 
 * It has a facade over key-value map
 * 
 * underlying implementation can be some tree implementation,
 * prefix tree, avl tree or hashmap.
 * 
 * FIXME: implementation is an unbalanced tree now
 * 
 * Author: peter
 *
 * Created on July 16, 2018, 7:35 PM
 */

#ifndef ASSOCIATIVE_ARRAY__H
#define ASSOCIATIVE_ARRAY__H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct aa_item_st {
        char * key;
        char * value;
        struct aa_item_st * left;
        struct aa_item_st * right;
    } aa_item;

    typedef struct aa_st {
        aa_item* root;
    } associative_array;
    
    void aa_put(associative_array* aa, char * key, char * value);
    int aa_has(aa_item* aa, char * key);
    aa_item* aa_get(aa_item* aa, char * key);
    void aa_free(associative_array * aa);
    void aa_merge(associative_array* target, aa_item* tree);
    char* aa_create_str_representation(aa_item* aa);


#ifdef __cplusplus
}
#endif

#endif /* ASSOCIATIVE_ARRAY__H */

