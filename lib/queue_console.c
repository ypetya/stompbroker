/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   queue_console.c
 * Author: peter
 *
 * Created on July 12, 2018, 2:57 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.c"


char* clone_str(char* src) {
    int len = strlen(src);
    char* text = emalloc(len + 1);
    strcpy(text, src);
    text[len] = '\0';
    return text;
}

int main(int argc, char const *argv[])
{
    char string[20];

    queue q;
    
    do
    {
        if(scanf("%20s", string)<0) break;
        enqueue(&q, clone_str(string));
        fflush(stdout);
    } while (strcmp(string, "quit") != 0);
    
    printf("\nQueue contains:\n\n");
    
    while(q.size > 0) {
        char * str = dequeue(&q);
        printf("%s\n",str);
        free(str);
    }
    
    return 0;
}
