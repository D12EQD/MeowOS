#include "kernel/lib/printk.h" 
#include "kernel/lib/string.h"
#include "kernel/mm/def.h"
#include "kernel/mm/pmm.h"

void* ok[16384];
int idx = 0;

void test1(){
    while (1){
        void * p = kalloc_page();
        if (!p) break;
        ok[idx ++] = p;
    }
    pr_info("idx is %d\n", idx);
    
    void* max_ram = 0;
    void* min_ram = (void *)(u64)(-1);
    for (int i = 0; i < idx; i ++){
        memset(ok[i], 0, PAGE_SIZE);
        max_ram = max(max_ram, (ok[i] + PAGE_SIZE - 1));
        min_ram = min(min_ram, (ok[i]));
    }
    pr_info("The mem range is [%p, %p]\n", min_ram, max_ram);
}

void test2(){
    for (int i = 0; i < idx; i ++){
        kfree(ok[i]);
    }
    idx = 0;
}
