
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
        general_list_item* first;
        general_list_item* last;
        int size;
    } general_list;

    general_list * list_new();
    void list_free(general_list* list);
    void list_clear(general_list* list);
    //void list_free_items(general_list* list);
    void list_print(general_list* list);
    void list_add(general_list* str_list, void* new_data);

    int list_index_of(general_list*list, void* data, size_t siz);
    int list_remove_at(general_list*list, int ix);
    
    general_list_item* list_get_at(general_list*list, int ix);
    // returns data
    void* list_unchain_at(general_list*list, int ix);

    general_list_item* create_general_list_item(void *data);

#ifdef __cplusplus
}
#endif

#endif /* STRING_LIST_H */

