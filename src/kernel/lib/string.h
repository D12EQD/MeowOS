#pragma once
#include "kernel/lib/def.h"

/* 获取字符串s长度，限制循环长度为limit */
size_t strnlen(const char * s, size_t limit);

size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);

char *strcpy(char *dst, const char *src);

u64 memcpy(void *dst, const void *src, size_t n);
u64 memset(void *buf, char c, size_t n);
