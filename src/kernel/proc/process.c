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
    user_unmap_pages(p->page_table, PROCESS_STACK_PAGE_START, PROCESS_STACK_PAGE_COUNT, true);
    free_walk(p->page_table, 2);
}

void create_idle_process(){
    // 0 号为空闲进程
    // TODO : 该进程需要做更多事情
    current_process = &process_list[0];
    current_process->state = PROC_READY;
    current_process->pid = 0;
}

void process_init(){
    for (u64 i = 0; i < PROCESS_MAX_COUNT; i ++) {
        process_list[i].state = PROC_UNUSED;
        process_list[i].kstack_bottom = KERNEL_STACK(i); // 该地址已经被内核代码映射完毕
        process_list[i].pid = i;
    }
    create_idle_process();
}

/* 创建用户进程页表 */
pt_t create_user_process_page(Process *p){
    pt_t page_table = user_alloc_page_table(); // user alloc page table 不一定成功
    if (!page_table) goto page_table_free;

    // 对于固定的代码段或者数据段，使用 mappages 分配，该代码不会调用页面分配算法
    // 对于动态的数据段，使用 user_map_pages 分配，该代码会调用页面分配算法

    // 1. trampoline（内核/用户共享）
    if (!mappages(page_table, TRAMPOLINE_VADDR, 1, (u64)trampoline, PTE_R | PTE_X))
        goto trampoline_free;

    // 2. trapframe
    p->trap_frame_page = kalloc_page();
    if (!p->trap_frame_page) goto trampoline_free;
    memset((void *)p->trap_frame_page, 0, PAGE_SIZE);

    if (!mappages(page_table, TRAPFRAME_VADDR, 1, (u64)p->trap_frame_page, PTE_R | PTE_W)) {
        kfree(p->trap_frame_page);
        p->trap_frame_page = NULL;
        goto trampoline_free;
    }
    
    // 3. 用户程序
    u64 page_count = (_binary_user_bin_end - _binary_user_bin_start + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page_count != user_map_pages(page_table, USER_VADDR_START, page_count, PTE_R | PTE_X)){
        goto trapframe_free;
    }

    // 复制用户程序到用户页表映射到的物理页
    u64 total = (u64)_binary_user_bin_end - (u64)_binary_user_bin_start;
    u64 src  = (u64)_binary_user_bin_start;
    u64 dst_va = USER_VADDR_START;
    while (total > 0) {
        u64 n = (total > PAGE_SIZE) ? PAGE_SIZE : total;
        u64 dst_pa = vaddr_to_paddr(page_table, dst_va);
        if (!dst_pa) goto user_free;
        memcpy((void *)dst_pa, (void *)src, n);
        src += n;
        dst_va += n;
        total -= n;
    }
    p->alloc_page_count += page_count;

    // 4. 用户栈
    if (user_map_pages(
         page_table, 
         PROCESS_STACK_PAGE_START,
         PROCESS_STACK_PAGE_COUNT, 
         PTE_R | PTE_W
    ) != PROCESS_STACK_PAGE_COUNT ){
        goto user_free;
    }

    return page_table;

// stack_free:
    user_unmap_pages(page_table, PROCESS_STACK_PAGE_START, PROCESS_STACK_PAGE_COUNT, true);
user_free:
    user_unmap_pages(page_table, USER_VADDR_START, page_count, true);
trapframe_free:
    user_unmap_pages(page_table, TRAPFRAME_VADDR, 1, true);
trampoline_free:
    user_unmap_pages(page_table, TRAMPOLINE_VADDR, 1, false);
page_table_free:
    if (page_table) free_walk(page_table, 2);
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
    
    // 2. create page_table for this process
    p->page_table = create_user_process_page(p);
    if (!p->page_table) goto free_trap_frame_page;
    
    // 3. update context
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
                pr_cont_debug("current_process is "); process_debug(current_process);
                p->state = PROC_RUNNING;
                Process *old = current_process;

                // debug
                pr_cont_debug("switch to the process : "); process_debug(p);

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
    pr_debug("[");
    pr_debug("  process_debug : pid = %ld", p->pid);
    pr_debug("  context infomation : ra = %lx, sp = %lx", p->context.ra, p->context.sp);
    pr_debug("  page_table = %lx, frame_page = %lx", p->page_table, p->trap_frame_page);
    pr_debug("]");

}

void trap_frame_debug(TrapFrame *tf){
     if (!tf) {
        pr_cont_debug("TrapFrame: NULL\n");
        return;
    }

    pr_cont_debug("TrapFrame @ 0x%lx {\n", (u64)tf);
    pr_cont_debug("  [  0] kernel_satp        = 0x%lx\n", tf->kernel_satp);
    pr_cont_debug("  [  8] kernel_sp          = 0x%lx\n", tf->kernel_sp);
    pr_cont_debug("  [ 16] user_trap_function = 0x%lx\n", tf->user_trap_function);
    pr_cont_debug("  [ 24] epc                = 0x%lx\n", tf->epc);
    pr_cont_debug("  [ 32] kernel_hartid      = 0x%lx\n", tf->kernel_hartid);

    pr_cont_debug("  [ 40] ra                 = 0x%lx\n", tf->ra);
    pr_cont_debug("  [ 48] sp                 = 0x%lx\n", tf->sp);
    pr_cont_debug("  [ 56] gp                 = 0x%lx\n", tf->gp);
    pr_cont_debug("  [ 64] tp                 = 0x%lx\n", tf->tp);
    pr_cont_debug("  [ 72] t0                 = 0x%lx\n", tf->t0);
    pr_cont_debug("  [ 80] t1                 = 0x%lx\n", tf->t1);
    pr_cont_debug("  [ 88] t2                 = 0x%lx\n", tf->t2);
    pr_cont_debug("  [ 96] s0                 = 0x%lx\n", tf->s0);
    pr_cont_debug("  [104] s1                 = 0x%lx\n", tf->s1);

    pr_cont_debug("  [112] a0                 = 0x%lx\n", tf->a0);
    pr_cont_debug("  [120] a1                 = 0x%lx\n", tf->a1);
    pr_cont_debug("  [128] a2                 = 0x%lx\n", tf->a2);
    pr_cont_debug("  [136] a3                 = 0x%lx\n", tf->a3);
    pr_cont_debug("  [144] a4                 = 0x%lx\n", tf->a4);
    pr_cont_debug("  [152] a5                 = 0x%lx\n", tf->a5);
    pr_cont_debug("  [160] a6                 = 0x%lx\n", tf->a6);
    pr_cont_debug("  [168] a7                 = 0x%lx\n", tf->a7);

    pr_cont_debug("  [176] s2                 = 0x%lx\n", tf->s2);
    pr_cont_debug("  [184] s3                 = 0x%lx\n", tf->s3);
    pr_cont_debug("  [192] s4                 = 0x%lx\n", tf->s4);
    pr_cont_debug("  [200] s5                 = 0x%lx\n", tf->s5);
    pr_cont_debug("  [208] s6                 = 0x%lx\n", tf->s6);
    pr_cont_debug("  [216] s7                 = 0x%lx\n", tf->s7);
    pr_cont_debug("  [224] s8                 = 0x%lx\n", tf->s8);
    pr_cont_debug("  [232] s9                 = 0x%lx\n", tf->s9);
    pr_cont_debug("  [240] s10                = 0x%lx\n", tf->s10);
    pr_cont_debug("  [248] s11                = 0x%lx\n", tf->s11);

    pr_cont_debug("  [256] t3                 = 0x%lx\n", tf->t3);
    pr_cont_debug("  [264] t4                 = 0x%lx\n", tf->t4);
    pr_cont_debug("  [272] t5                 = 0x%lx\n", tf->t5);
    pr_cont_debug("  [280] t6                 = 0x%lx\n", tf->t6);
    pr_cont_debug("}\n");
}
