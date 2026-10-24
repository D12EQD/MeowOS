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

void switch_ctx(ThreadCtx *c1, ThreadCtx* c2);

Process process_list[PROCESS_MAX_COUNT]; /* 定义进程控制块 */
Cpu cpu_list[CPU_NUMBER];

Process* idle_process;
Process* current_process;

static inline void process_free_page_table(Process* p){
    user_unmap_pages(p->page_table, TRAPFRAME_VADDR, 1, true);
    user_unmap_pages(p->page_table, TRAMPOLINE_VADDR, 1, true);
    user_unmap_pages(p->page_table, USER_VADDR_START, p->alloc_page_count, true);
    free_walk(p->page_table, 2);
}

static inline void idle_loop(){
    pr_warn("cpu idle");
    while (1){
        WAITING_INT;
    }
}

void process_init(){
    for (u64 i = 0; i < PROCESS_MAX_COUNT; i ++) {
        process_list[i].kstack = KERNEL_STACK(i);
        process_list[i].state = PROC_UNUSED;
        process_list[i].pid = i;
        spinlock_init(&process_list[i].lock);
    }
    idle_process = create_user_process((u64)idle_loop);
    current_process = idle_process;
}

pt_t create_process_page(Process *p){
    pt_t page_table = user_alloc_page_table();
    bool ret = 0;

    if (!page_table) goto page_table_free;
    
    ret = mappages(page_table, TRAMPOLINE_VADDR, 1, (u64)trampoline, PTE_R | PTE_X);
    if (!ret) goto trampoline_free;
    
    ret = mappages(page_table, TRAPFRAME_VADDR, 1, (u64)(p->trap_frame_page), PTE_R | PTE_W);
    if (!ret) goto trapframe_free;

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
 * 创建一个进程
 * 该程序会关闭中断，防止其他线程切换和抢占
 */
Process *create_user_process(u64 pc) {
    int idx = -1;

    for (int i = 0; i < PROCESS_MAX_COUNT; i ++){
        spinlock_acquire(&process_list[i].lock);
        if (process_list[i].state == PROC_UNUSED){
            idx = i;
            break;
        }
        spinlock_release(&process_list[i].lock);
    }
    
    if (idx == -1) goto return_null;
    // 1. init 
    Process* p = &process_list[idx];
    p->state = PROC_READY; // 进程状态
    p->alloc_page_count = 0;
    
    // 2. alloc tram frame
    p->trap_frame_page = kalloc_page();
    if (!p->trap_frame_page) goto return_null;
    
    // 3. create page_table for this process
    p->page_table = create_process_page(p);
    if (!p->page_table) goto free_trap_frame_page;
    
    // 4. update context
    memset(&(p->context), 0, sizeof(ThreadCtx));
    p->context.sp = p->kstack + PAGE_SIZE;
    p->context.ra = (u64)pc;

    // 5. user_trap_handler
    p->trap_frame_page->user_trap_function = (u64)user_trap_handler;
    
    spinlock_release(&process_list[idx].lock);
    return p;

free_trap_frame_page:
    kfree(p->trap_frame_page);
return_null:
    spinlock_release(&process_list[idx].lock);
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
 * 找到空闲进程并且上锁
 * 需要调用 spinlock_release 对进程解锁
 */
Process* find_process(){
    int idx = (current_process->pid + 1) % PROCESS_MAX_COUNT;

    for (int i = 0; i < PROCESS_MAX_COUNT; i ++, idx = (idx + 1) % PROCESS_MAX_COUNT){
        if (idx == 0) continue; // 跳过空闲进程
        spinlock_acquire(&process_list[idx].lock);
        if (process_list[idx].state == PROC_READY){
            return &process_list[idx];
        }
        spinlock_release(&process_list[idx].lock);
    }

    spinlock_acquire(&idle_process->lock);
    return idle_process;
}

/*
 * 进程让出
 */
void yield(void){
    Process *proc = find_process();

    if (proc == current_process){
        pr_debug("Do not change the current process");
        spinlock_release(&proc->lock);
        return;
    }

    current_process = proc;
    spinlock_release(&proc->lock);

    switch_ctx(&current_process->context, &proc->context);
}

// void schedule(){
//     Process *p;
//     Cpu *cpu = get_cpu();
//
//     while (1){
//         interrupt_on();
//         pr_debug("schedule interrupt on and off"); // TODO: just for test
//         interrupt_off();
//
//         int idx = -1;
//         for (int i = 0; i < PROCESS_MAX_COUNT; i ++){
//             spinlock_acquire(&process_list[i].lock);
//             if (process_list[i].state == PROC_READY){
//                 p = &process_list[i];
//                 idx = i;
//
//                 p->state = PROC_RUNNING;
//                 cpu->proc = p;
//
//                 switch_ctx(&p->context, &cpu->context);
//
//                 get_cpu()->enable_intr = false; // 强制关闭中断
//
//             }
//             cpu->enable_intr = true;
//             spinlock_release(&process_list[i].lock);
//         }
//
//         if (idx == -1){
//             WAITING_INT;
//             continue;
//         }
//
//     }
// }

void print_process(Process *p){
    pr_debug("pid = %d", p->pid);
}

