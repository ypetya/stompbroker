
#ifndef GENERAL_LIST_H
#define GENERAL_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct general_item {
        void* data;
        struct general_item* next;
    } general_list_item;

    typedef struct general_list_st {
        general_list_item* list;
    } general_list;

    void list_free_items(general_list* list);
    void list_print(general_list* list);
    void list_add(general_list* str_list, void* new_data);

#ifdef __cplusplus
}
#endif

#endif /* STRING_LIST_H */

