#include "kernel/arch/def.h"

#include "kernel/proc/process.h"
#include "kernel/mm/page.h"
#include "kernel/mm/def.h"
#include "kernel/mm/pmm.h"

#include "kernel/lib/printk.h"
#include "kernel/lib/string.h"

EXTERN_SYMBOL(__start_text_section);
EXTERN_SYMBOL(__end_text_section);

EXTERN_SYMBOL(__trampoline_section_start);
EXTERN_SYMBOL(__trampoline_section_end);

EXTERN_SYMBOL(__start_other_section);
EXTERN_SYMBOL(__end_other_section);

EXTERN_SYMBOL(__free_ram);
EXTERN_SYMBOL(__free_ram_end);

pt_t kernel_page_table; /* 内核页目录 */

static inline void print_pte(pte_t pte){
    pr_debug("%lx %b", PTE_TO_PADDR(pte), (pte & 0b1111111111));
}

static inline void print_vaddr(u64 vaddr){
    pr_debug("%lx %lx %lx %lx", GET_VA_VPN2(vaddr), GET_VA_VPN1(vaddr), GET_VA_VPN0(vaddr), GET_VA_OFFSET(vaddr));
}

static inline void flush_TLB(){
    _ASM("sfence.vma zero, zero" ::: "memory");
}

/* kernel 开启分页 pnn 根页表物理地址 */
void kernel_page_open(){
    flush_TLB();
    WRITE_CSR(CSR_SATP, MAKE_SATP(kernel_page_table));
    flush_TLB();
}

/* 分配一个页，给页表，一定可以分配并且自动清空 */
void* kernel_alloc_page_table(){
    void* res = kalloc_page();
    ASSERT(res != NULL);
    memset(res, 0, PAGE_SIZE);
    return res;
}

void *user_alloc_page_table(){
    void *res = kalloc_page();
    if (!res) return NULL;
    memset(res, 0, PAGE_SIZE);
    return res;
}

/* 
 * level-2页表 page_table 创建一个虚拟地址
 * args:
 *      page_table level-2 page table 虚拟地址
 *      is_alloc 是否可以分配物理页面给新地址
 *      vaddr 需要分配额的虚拟地址
 * return:
 *      返回该页表项的指针
 * 30..38 -- 9 bits of level-2 index.
 * 21..29 -- 9 bits of level-1 index.
 * 12..20 -- 9 bits of level-0 index.
 * 0..11 -- 12 bits of byte offset within the page.
 */
pte_t* walk(pt_t page_table, bool is_alloc, u64 vaddr){
    if (vaddr >= MAX_VADDR){
        PANIC("out of range");
    }
    if (GET_VA_RESERVE(vaddr)){
        PANIC("virtual address error");
    }
    
    pt_t pt = page_table;
    for (int level = 2; level >= 1; level --){
        pte_t* next = &pt[GET_VA_VPN(vaddr, level)];
        
        if (*next & PTE_V){
            pt = PTE_TO_PADDR(*next);
        }else{
            if (is_alloc){
                u64 new_page = (u64)kernel_alloc_page_table();
                if (!new_page) goto failed;
                
                // memset((void *)new_page, 0, PAGE_SIZE);
                *next = PADDR_TO_PTE(new_page) | PTE_V;
                pt = (pt_t) new_page;
            }else{
                goto failed;
            }
        }
    }
    
    return &pt[GET_VA_VPN(vaddr, 0)];
failed:
    pr_warn("walk failed\n");
    return NULL;
}


/* 
 * 一般用于用户页释放，比如释放进程
 * 递归释放页表和页表项上的数据 
 * depth 指定最大的递归层数 如果为负数会一直dfs
 */
void free_walk(pt_t page_table, int depth){
    if (depth == 0){
        return;
    }

    for (u64 i = 0; i < PAGE_SIZE / sizeof(pte_t); i ++){
        pte_t pte = (pte_t)page_table[i];
        if (pte & PTE_V){
            if (pte & (PTE_R | PTE_W | PTE_X)){ // is a page_table
                pt_t child_page_table = (pt_t) PTE_TO_PADDR(pte);
                free_walk(child_page_table, depth - 1);
                page_table[i] = 0;
            }else{
                PANIC("error page");
            }
        }
    }

    kfree((void *)page_table);
}

/* 
 * 把一段连续的虚拟地址 va，映射到一段连续的物理地址 paddr 上
 * permit_flag 不需要设置 PTE_V 会自动设置
 * returns: ture or false
 */
bool mappages(pt_t page_table, u64 va, u64 page_count, u64 paddr, u64 permit_flag) {
    u64 vaddr;
    pte_t* pte;

    if unlikely((va % PAGE_SIZE) != 0) PANIC("mappages: va not aligned");
    if unlikely(page_count == 0) PANIC("mappages: size");
    
    // 遍历所有 level-0 页表
    for (vaddr = va; vaddr < va + page_count * PAGE_SIZE; vaddr += PAGE_SIZE, paddr += PAGE_SIZE) {
        pte = walk(page_table, true, vaddr);

        if (pte == NULL) return false;
        if unlikely(*pte & PTE_V) PANIC("mappages: remap");

        *pte = PADDR_TO_PTE(paddr) | permit_flag | PTE_V;
    }

    return true;
}

/* 分配内核页面 保证一定可以分配*/
void kernel_mappages(pt_t page_table, u64 va, u64 size, u64 paddr, u64 permit_flag){
    ASSERT(size % PAGE_SIZE == 0);
    ASSERT(mappages(page_table, va, size >> PAGE_SIZE_LOG2, paddr, permit_flag) == true);
}

/* 初始化内核页面 */
void kernel_page_init(){
    // 分配level-2页表
    kernel_page_table = kernel_alloc_page_table();

    // 分配代码段
    kernel_mappages(
        kernel_page_table, 
        (u64)__start_text_section, 
        (u64)__end_text_section - (u64)__start_text_section, 
        (u64)__start_text_section, PTE_R | PTE_X
    );

    // 分配 trampoline
    kernel_mappages(
        kernel_page_table, 
        TRAMPOLINE_VADDR,
        (u64)__trampoline_section_end - (u64)__trampoline_section_start,
        (u64)__trampoline_section_start, PTE_R | PTE_X
    );
    
    // 分配只读区域
    kernel_mappages(
        kernel_page_table,
        (u64)__start_other_section,
        (u64)__end_other_section - (u64)__start_other_section,
        (u64)__start_other_section, PTE_R | PTE_W
    );

    // 分配所有物理内存
    kernel_mappages(kernel_page_table,
        (u64)__free_ram,
        (u64)__free_ram_end - (u64)__free_ram,
        (u64)__free_ram, PTE_R | PTE_W
    );
}



/*
 * 根据虚拟地址和 level-2页表 找到物理地址
 * 可以适用于内核和用户页
 * returns:
 *      if failed, return 0
 *      if success, return paddr
 */
u64 vaddr_to_paddr(pt_t page_table, u64 vaddr){
    if (vaddr >= MAX_VADDR){
        // pr_debug("out of range");
        return 0;
    }
    if (GET_VA_RESERVE(vaddr)){
        // pr_debug("virtual address error");
        return 0;
    }
    
    pt_t pt = page_table;
    // pr_debug("vaddr = "); print_vaddr(vaddr); pr_debug("\n");

    for (int level = 2; level >= 0; level --){
        pte_t* next = &pt[GET_VA_VPN(vaddr, level)];
        
        // print_pte(*next); pr_debug("\n");
        if (*next & PTE_V){
            pt = PTE_TO_PADDR(*next);
        }else{
            goto failed;
        }
    }

    return (u64)pt + GET_VA_OFFSET(vaddr);
failed:
    pr_debug("physical address get failed\n");
    return 0;
}

/*
 * 为用户分配 count 个页面
 * args:
 *      page_table : 用户level-2页表
 *      vaddr : 需要映射到的虚拟地址
 *      count : 需要分配的页数量
 *      perm : 许可位
 * returns:
 *      u64 : 分配页面数量，可能小于 count
 */
u64 user_map_pages(pt_t page_table, u64 vaddr, u64 count, u64 perm){
    if (!page_table) return 0;
    
    u64 res = 0;
    for (u64 i = 0; i < count; i ++){
        void *new = kalloc_page();
        if (!new) break;
        // 防御一下 防止调用者没用使用 PTE_U
        if (mappages(page_table, vaddr, 1, (u64)new, PTE_U | perm) == false) break; 
        res ++;
    }

    return res;
}

/*
 * 取消映射用户页表
 * is_free 标志是否需要释放页表，调用kfree释放
 */
void user_unmap_pages(pt_t page_table, u64 start, u64 page_count, bool is_free){
    if (start % PAGE_SIZE){
        PANIC("user_unmap_pages : start is not align with page_size");
    }
    
    for (u64 i = start; i < page_count * PAGE_SIZE + start; i += PAGE_SIZE){
        pte_t * pte = walk(page_table, false, i);
        if (!pte){
            pr_warn("vaddr %lu do not existed in this page_table\n", i);
            continue;
        }

        if (*pte & PTE_V){
            pr_warn("vaddr %lu is not a vaild address in this page_table\n", i);
            continue;
        }

        if (is_free){
            kfree((void *)( 
                 PTE_TO_PADDR((u64)( *pte )) 
            ));
        }

        *pte = 0;
    }
}

