#pragma once
#include "lib/def.h"
/* This file was automatically generated.  Do not edit! */
int printf(const char *fmt,...);
int sprintf(char *buf,const char *fmt,...);
int vsprintf(char *buf,const char *fmt,va_list args);

#define PANIC(fmt, ...)                                                    \
do {                                                                       \
    printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);  \
    while (1) {}                                                           \
} while (0)

