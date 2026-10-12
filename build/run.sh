#!/usr/bin/fish

set -l prefix "build/src"
gcc -std=c23                \
    -o build/main -I$prefix \
    $prefix/main.c          \
    $prefix/nob.c           \
    $prefix/flaglist.c      \
