#!/bin/bash

for file in string_list_console.c\
 random_string_console.c ; do
    gcc -v "$file" -o "${file/.c/.out}"
done

echo 'Done!'