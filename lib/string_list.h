
#ifndef STRING_LIST_H
#define STRING_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct string_item {
        char* word;
        struct string_item* next;
    } string_list_item;

    typedef struct string_list_st {
        string_list_item* list;
    } string_list;

    void string_list_free_items(string_list* list);
    void string_list_print(string_list* list);
    void string_list_add(string_list* str_list, char* new_str);


#ifdef __cplusplus
}
#endif

#endif /* STRING_LIST_H */

