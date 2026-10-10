#pragma once
#include "lib/def.h"

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *buf, char c, size_t n);
paddr_t alloc_pages(u32 n);
