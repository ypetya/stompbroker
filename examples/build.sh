#!/bin/bash

for file in *.c ; do
    gcc -v "$file" -o "${file/.c/.out}"
done

echo 'Done!'