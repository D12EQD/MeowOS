// 古希腊掌管物理内存的代码
#include "kernel/arch/def.h"

#include "kernel/mm/def.h"
#include "kernel/mm/pmm.h"

#include "kernel/lib/def.h"
#include "kernel/lib/string.h"
#include "kernel/lib/linklist.h"
#include "kernel/lib/printk.h"
#include "kernel/lib/bitmap.h"

#include "kernel/proc/lock.h"

#define REF_LIST_SIZE   1024

EXTERN_SYMBOL(__bss);
EXTERN_SYMBOL(__bss_end);

EXTERN_SYMBOL(__free_ram);
EXTERN_SYMBOL(__free_ram_end);

static List free_list = {0};
static List free_ref_list = {0};

SPINLOCK_GLOBAL_INIT(pmm_lock);

/* bss 初始化 */
void bss_init(){
    memset(__bss, 0, (u64) __bss_end - (u64) __bss);  // bss初始化为0
}

/* 
 * 内核物理页面初始化 
 */
void kmm_init(){
    for (u64 i = ((u64)(__free_ram)); i < (u64)__free_ram_end; i += PAGE_SIZE){
        kfree((void *)i);
    }
    for (int i = 0; i < REF_LIST_SIZE; i ++){
        list_insert_front(&free_ref_list, kalloc_page());
    }
}

/* 释放一个物理页框的内存 */
void kfree(void* paddr){
    spinlock_acquire(&pmm_lock);

    if unlikely((((u64)paddr) % PAGE_SIZE != 0) || (paddr >= (void *)__free_ram_end) || (paddr < (void *)__free_ram)){
        PANIC("kfree failed");
    }

    memset(paddr, 1, PAGE_SIZE); // make an error page
    list_insert_front(&free_list, paddr);

    spinlock_release(&pmm_lock);
    return;
}


/* 分配一个物理页框 或 一个普通页面，不取决于是否开启分页，该系统中内核页面和物理页面虚拟地址等价*/
void *kalloc_page(void){
    spinlock_acquire(&pmm_lock);

    if unlikely(list_is_empty(&free_list)) {
        pr_warn("phsical memory is used up\n");
        spinlock_release(&pmm_lock);
        return NULL;
    }

    void *paddr = (void*)(free_list.begin); // head 
    list_delete_front(&free_list);
    
    spinlock_release(&pmm_lock);

    return paddr;
}
