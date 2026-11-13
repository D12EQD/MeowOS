#pragma once
#include "user/def.h"

#define ANSI_RESET          "\033[0m"
#define ANSI_RED            "\033[31m"
#define ANSI_GREEN          "\033[1;32m"
#define ANSI_YELLOW         "\033[33m"
#define ANSI_BLUE           "\033[34m"
#define ANSI_MAGENTA        "\033[35m"
#define ANSI_CYAN           "\033[36m"
#define ANSI_BRIGHT_RED     "\033[1;31m"
#define ANSI_BRIGHT_YELLOW  "\033[1;33m"
#define ANSI_BRIGHT_GREEN   "\033[1;32m"

#define println(fmt, ...)   printf_imple(ANSI_YELLOW fmt ANSI_RESET "\n", ##__VA_ARGS__)
#define printf(fmt, ...)    printf_imple(ANSI_YELLOW fmt ANSI_RESET, ##__VA_ARGS__)

int printf_imple(const char *fmt, ...);

#define STDOUT  1
#define STDIN   0

