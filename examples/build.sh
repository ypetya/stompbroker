#!/bin/bash

for file in *.c ; do
    gcc -v "$file" -o "${file/.c/.out}" -lpthread
done

echo 'Done!'