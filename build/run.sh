#!/usr/bin/fish

set -l prefix "build/src"
gcc -o build/main -I$prefix \
    $prefix/main.c  \
    $prefix/nob.c   \
    $prefix/pathlist.c  \
    $prefix/arch/build.c \
    $prefix/mm/build.c \
    $prefix/lib/build.c \
    $prefix/kernel/build.c \
