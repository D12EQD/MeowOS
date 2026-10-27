#include "kernel/proc/process.h"
#include "kernel/proc/lock.h"

#include "kernel/arch/def.h"
#include "kernel/arch/trap.h"

#include "kernel/lib/string.h"
#include "kernel/lib/def.h"
#include "kernel/lib/printk.h"

#include "kernel/mm/def.h"
#include "kernel/mm/page.h"
#include "kernel/mm/pmm.h"

EXTERN_SYMBOL(trampoline); // 蹦床代码，用于页表映射
EXTERN_SYMBOL(_binary_user_bin_start);
EXTERN_SYMBOL(_binary_user_bin_end);

EXTERN_FUNC(void switch_ctx(ThreadCtx *old, ThreadCtx* new));
EXTERN_FUNC(void user_trap_entry(void *));
EXTERN_FUNC(void user_trap_ret(void *));

Process process_list[PROCESS_MAX_COUNT];
Process *current_process;

static inline void process_free_page_table(Process* p){
    user_unmap_pages(p->page_table, TRAPFRAME_VADDR, 1, true);
    user_unmap_pages(p->page_table, TRAMPOLINE_VADDR, 1, true);
    user_unmap_pages(p->page_table, USER_VADDR_START, p->alloc_page_count, true);
    free_walk(p->page_table, 2);
}

void create_idle_process(){
    // 0 号为空闲进程
    current_process = &process_list[0];
    current_process->state = PROC_READY;
    current_process->pid = 0;
}

void process_init(){
    for (u64 i = 0; i < PROCESS_MAX_COUNT; i ++) {
        process_list[i].kstack_bottom = KERNEL_STACK(i);
        process_list[i].state = PROC_UNUSED;
        process_list[i].pid = i;
    }
    create_idle_process();
}

/* 创建用户进程页表 */
pt_t create_user_process_page(Process *p){
    pt_t page_table = user_alloc_page_table();
    bool ret = 0;

    if (!page_table) goto page_table_free;
    
    ret = mappages(page_table, TRAMPOLINE_VADDR, 1, (u64)trampoline, PTE_R | PTE_X);
    if (!ret) goto trampoline_free;
    
    ret = mappages(page_table, TRAPFRAME_VADDR, 1, (u64)(p->trap_frame_page), PTE_R | PTE_W);
    if (!ret) goto trapframe_free;

    u64 page_count = _binary_user_bin_end - _binary_user_bin_start + PAGE_SIZE - 1;
    page_count /= PAGE_SIZE;
    
    // 复制用户程序到真实物理空间
    ASSERT(page_count == user_map_pages(page_table, USER_VADDR_START, page_count, PTE_R | PTE_X));
    for (u64 i = 0, va = USER_VADDR_START; i < page_count; i ++, va += PAGE_SIZE){
        u64 pa = vaddr_to_paddr(page_table, va); // 通过虚拟地址找到物理地址
                                                 // 注意内核的虚拟地址和物理地址等价
        memcpy((void *)pa, _binary_user_bin_start + i * PAGE_SIZE, PAGE_SIZE);
    }

    p->alloc_page_count += page_count;

    return page_table;

    // 注意需要尝试释放，因为需要释放分配过程中被分配的页表
trapframe_free:
    user_unmap_pages(page_table, TRAPFRAME_VADDR, 1, true);
trampoline_free:
    user_unmap_pages(page_table, TRAMPOLINE_VADDR, 1, true);
page_table_free:
    if (page_table){
        free_walk(page_table, 2);
    }
    return NULL;
}

/*
 * 创建用户进程
 */
Process *create_user_process() {
    // 1. init 
    int idx = -1;
    for (int i = 0; i < PROCESS_MAX_COUNT; i ++){
        // spinlock_acquire(&process_list[i].lock);
        if (process_list[i].state == PROC_UNUSED){
            idx = i;
            break;
        }
    }

    if (idx == -1) goto return_null;
    Process *p = &process_list[idx];

    p->state = PROC_READY; // 进程状态
    p->alloc_page_count = 0;
    
    // 2. plloc tram frame
    p->trap_frame_page = kalloc_page();
    if (!p->trap_frame_page) goto return_null;
    
    // 3. create page_table for this process
    p->page_table = create_user_process_page(p);
    if (!p->page_table) goto free_trap_frame_page;
    
    // 4. update context
    memset(&(p->context), 0, sizeof(ThreadCtx));
    p->context.ra = (u64)fork_ret;
    p->context.sp = p->kstack_bottom + PAGE_SIZE;

    return p;

free_trap_frame_page:
    kfree(p->trap_frame_page);
return_null:
    return NULL;
}

/*
 * 释放一个用户进程 释放该用户分配的页表
 */
void free_user_process(Process* p){
    p->state = PROC_UNUSED;

    process_free_page_table(p);
    p->page_table = NULL;
    p->trap_frame_page = NULL;
    
    p->alloc_page_count = 0;
}

/*
 * 找到可以执行的进程并且上锁
 * 需要调用 spinlock_release 对进程解锁
 */
Process* find_ready_process(){
    int idx = (current_process->pid + 1) % PROCESS_MAX_COUNT;
    for (int i = 0; i < PROCESS_MAX_COUNT; i ++, idx = (idx + 1) % PROCESS_MAX_COUNT){
        if (idx == 0) continue; // 优先跳过空闲进程

        // 尝试获取当前可以运行进程的锁
        // 如果发现进程持有锁（比如正在运行）跳过
        // if (!spinlock_try_acquire(&process_list[idx].lock)){
        //     continue;
        // }

        if (process_list[idx].state == PROC_READY){
            // pr_cont_debug("find_ready_process %idx : ", idx); process_debug(&process_list[idx]);
            // spinlock_debug(&process_list[idx].lock);
            return &process_list[idx];
        }

        // spinlock_release(&process_list[idx].lock);
    }

    return &process_list[0];
}

// Give up the CPU for one scheduling round.
void yield(void) {
    Process *p = current_process;
    p->state = PROC_READY;
    switch_ctx(&p->context, &current_process->context);
}

void scheduler(void) {
    Process *p;

    while (1){
        interrupt_on();
        interrupt_off();
        
        bool found = false;
        int pid;
        
        // 跳过空闲进程，找到一个可以执行的合适进程
        for (pid = 1; pid < PROCESS_MAX_COUNT; pid ++) {
            p = &process_list[pid];
            if (p->state == PROC_READY) {
                p->state = PROC_RUNNING;
                Process *old = current_process;

                // debug
                pr_debug("switch to the process : %d", pid); process_debug(p);

                current_process = p;
                switch_ctx(&old->context, &current_process->context);

                found = true;
            }
        }
        if (found == false) {
            // 空闲
            pr_warn("cpu is idle");
            _ASM("wfi");
        }
    }
}

void process_debug(Process *p){
    pr_debug("process_debug : pid = %ld", p->pid);
    pr_debug("context infomation : ra = %lx, sp = %lx", p->context.ra, p->context.sp);
}


