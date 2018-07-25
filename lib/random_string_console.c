/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   random_string_console.c
 * Author: peter
 *
 * Created on July 11, 2018, 4:49 PM
 */

#include <stdio.h>
#include <stdlib.h>

#include "random_string.c"
int main(int argc, char* argv[]) {

    if(argc<2) return EXIT_FAILURE;
    
    int num = atoi(argv[1]);
    
    print_random_str(num);
    
    return (EXIT_SUCCESS);
}

