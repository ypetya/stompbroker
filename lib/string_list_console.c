#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clone_str.c"
#include "string_list.c"

int main(int argc, char const *argv[])
{
    char string[20];

    string_list list;
    
    do
    {
        printf(">");
        printf("Enter + string[20]:");
        if(scanf("%20s", string)<0) break;
        printf("\nadd:%s %d\n", string, (int)strlen(string));
        string_list_add(&list,string);
        fflush(stdout);
    } while (strcmp(string, "quit") != 0);
    
    string_list_print(&list);
    string_list_free_items(&list);

    return 0;
}
