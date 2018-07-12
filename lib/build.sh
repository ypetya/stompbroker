#!/bin/bash

for file in *_console.c ; do
    gcc -v "$file" -o "${file/.c/.out}"
done

echo 'Done!'
