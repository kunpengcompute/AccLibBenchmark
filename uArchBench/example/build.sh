#!/bin/bash

echo "Compiling page boundary load test..."

gcc -O2 -march=armv8-a+sve -o page_boundary_load page_boundary_load.c

if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    echo "Run with: ./page_boundary_load [iterations]"
    echo "Example: ./page_boundary_load 10000000"
else
    echo "Compilation failed!"
    exit 1
fi
