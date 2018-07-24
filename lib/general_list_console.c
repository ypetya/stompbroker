/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   general_list_console.c
 * Author: peter
 *
 * Created on July 24, 2018, 3:28 PM
 */

#include <stdio.h>
#include <stdlib.h>

#include "emalloc.c"
#include "general_list.c"

/*
 * 
 */
int main(int argc, char** argv) {

    general_list *l = emalloc(sizeof (general_list));

    list_add(l, "Pimpa1");
    list_add(l, "Pimpa22");
    list_add(l, "Pimpa333");

    list_print(l);

    list_unchain_at(l, 1);
    printf("removed 1.\n");
    list_print(l);

    list_add(l, "Pitty");
    list_add(l, "Platty");
    list_add(l, "Plutty");
    list_print(l);
    
    list_unchain_at(l, 4);

    list_print(l);

    list_unchain_at(l, 0);
    list_unchain_at(l, 1);
    list_unchain_at(l, 0);
    list_unchain_at(l, 0);
    list_print(l);

}

