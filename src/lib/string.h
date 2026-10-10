#pragma once
#include "lib/def.h"

/* 获取字符串s长度，限制循环长度为limit */
size_t strnlen(const char * s, int limit);

size_t strlen(const char *s);

void puts(char *s);

int strcmp(const char *s1, const char *s2);

char *strcpy(char *dst, const char *src);

