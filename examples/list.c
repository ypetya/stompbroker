#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct elem {
    char* word;
    struct elem* next;
} Item;

Item* list_add(Item* list,Item* new_elem) {
    //new_elem->next=list;
    //return new_elem;
    if(list==NULL) return new_elem;

    Item* cur=list;
    while(cur->next!=NULL) cur=cur->next;
    cur->next=new_elem;
    return list;
}

void list_print(Item* list) {
    int i=0;
    Item* c=list;

    while(c!=NULL) {
        printf("%d. %s\n",i++,c->word);
        c=c->next;
    }
}

void list_free(Item* list) {
    Item* c=list;
    Item* n;

    while(c!=NULL) {
        n=c->next;
        free(c->word);
        free(c);
        c=n;
    }
}

char* clone_str(char* src) {
    int len =strlen(src);
    char* text = malloc(len+1);
    strcpy(text,src);
    text[len]='\0';
    return text;
}

Item* item_create(char *src) {
    Item* ptr= malloc(sizeof(Item));
    ptr->word = clone_str(src);
    return ptr;
}

int main(int argc, char const *argv[])
{
    char string[20];

    Item* list;
    
    do
    {
        printf("Enter string[20]:");
        scanf("%20s", string);
        printf("\nadd:%s\n", string);
        Item* new_item = item_create(string);
        list=list_add(list,new_item);
        fflush(stdout);
    } while (strcmp(string, "quit") != 0);
    
    list_print(list);
    list_free(list);

    return 0;
}
