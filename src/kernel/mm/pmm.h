#pragma once
#include "kernel/mm/def.h"

#include "kernel/lib/def.h"
#include "kernel/lib/linklist.h"

void bss_init();
void kmm_init();
void kfree(void* _paddr);
void *kalloc_page(void);


