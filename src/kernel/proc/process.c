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
EXTERN_SYMBOL(switch_ctx); // 上下切换代码

Process process_list[PROCESS_MAX_COUNT]; /* 定义进程控制块 */
Cpu cpu_list[CPU_NUMBER];

#define get_cpuid() ({ int id = READ_REG(tp);  id; })

static inline void process_free_page_table(Process* p){
    user_unmap_pages(p->page_table, TRAPFRAME_VADDR, 1, true);
    user_unmap_pages(p->page_table, TRAMPOLINE_VADDR, 1, true);
    user_unmap_pages(p->page_table, UESR_VADDR_START, p->alloc_page_count, true);
    free_walk(p->page_table, 2);
}

/* 获取当前cpu绑定的struct Cpu */
Cpu* get_cpu(){
    int id = get_cpuid();
    pr_info("cpu id is %d\n", id);
    return &cpu_list[id];
}

void process_init(){
    for (u64 i = 0; i < PROCESS_MAX_COUNT; i ++) {
        process_list[i].kstack = KERNEL_STACK(i);
        process_list[i].state = PROC_UNUSED;
        process_list[i].pid = i;
    }
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
Process *create_user_process() {
    int idx = -1;

    critical_enter();
    for (int i = 0; i < PROCESS_MAX_COUNT; i ++){
        if (process_list[i].state == PROC_UNUSED){
            idx = i;
            break;
        }
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

    critical_exit();
    return p;

free_trap_frame_page:
    kfree(p->trap_frame_page);
return_null:
    critical_exit();
    return NULL;
}

/*
 * 释放一个用户进程 释放该用户分配的页表
 */
void free_user_process(Process* p){
    p->state = PROC_UNUSED;
    process_free_page_table(p);
    kfree(p->trap_frame_page);
    p->trap_frame_page = NULL;

    kfree(p->page_table);
    p->page_table = NULL;
    
    p->alloc_page_count = 0;
}

void yield(void){

}

void schedule(){
    critical_enter();
    while (1){

    }
    critical_exit();
}

