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
    user_unmap_pages(p->page_table, TRAMPOLINE_VADDR, 1, false);
    user_unmap_pages(p->page_table, USER_VADDR_START, p->alloc_page_count, true);
    user_unmap_pages(p->page_table, PROCESS_STACK_PAGE_START, PROCESS_STACK_PAGE_COUNT, true);
    free_walk(p->page_table, 2);
}

void process_debug(Process *p){
    pr_debug("[");
    if (p->name) pr_debug("  process name : %s", p->name);
    pr_debug("  process_debug : pid = %d", p->pid);
    pr_debug("  context infomation : ra = %lx, sp = %lx", 
        p->context.ra, p->context.sp);
    pr_debug("  page_table = %lx, frame_page = %lx", 
        (u64)p->page_table, (u64)p->trap_frame_page
    );
    pr_debug("]\n");
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
        process_list[i].is_killed = false;
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

    // 3. 用户栈
    u64 page_count = user_map_pages(
        page_table, 
        PROCESS_STACK_PAGE_START,
        PROCESS_STACK_PAGE_COUNT, 
        PTE_R | PTE_W
    );

    if (page_count != PROCESS_STACK_PAGE_COUNT){
        goto stack_free;
    }

    p->alloc_page_count = 0;
    p->trap_frame_page->sp = PROCESS_STACK_PAGE_END;

    return page_table;

stack_free:
    user_unmap_pages(page_table, PROCESS_STACK_PAGE_START, PROCESS_STACK_PAGE_COUNT, true);
// trapframe_free:
    user_unmap_pages(page_table, TRAPFRAME_VADDR, 1, true);
trampoline_free:
    user_unmap_pages(page_table, TRAMPOLINE_VADDR, 1, false);
page_table_free:
    if (page_table) free_walk(page_table, 2);
    return NULL;
}

bool first_user_init(Process *proc){
    ASSERT(proc->pid == 1);
    ASSERT(proc->page_table);
    pt_t page_table = proc->page_table;

    // 用户程序 copy
    u64 page_count = (_binary_user_bin_end - _binary_user_bin_start + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page_count != user_map_pages(page_table, USER_VADDR_START, page_count, PTE_R | PTE_X)){
        goto error;
    }

    // 复制用户程序到用户页表映射到的物理页
    u64 total = (u64)_binary_user_bin_end - (u64)_binary_user_bin_start;
    u64 src  = (u64)_binary_user_bin_start;
    u64 dst_va = USER_VADDR_START;
    while (total > 0) {
        u64 n = (total > PAGE_SIZE) ? PAGE_SIZE : total;
        u64 dst_pa = vaddr_to_paddr(page_table, dst_va);
        if (!dst_pa) goto error;
        memcpy((void *)dst_pa, (void *)src, n);
        src += n;
        dst_va += n;
        total -= n;
    }

    proc->trap_frame_page->epc = USER_VADDR_START;
    proc->alloc_page_count += page_count ;
    proc->name = "first_process";
    proc->father = NULL;
    return true;
error:
    PANIC("First User error");
    return false;
}

/*
 * 创建用户进程
 * 首先寻找一个空闲位置存放新进程，为该进程创建新页表
 * 并且更新上下文返回位置为 fork_ret
 */
Process *create_user_process() {
    // 1. init 
    int idx = -1;
    for (int i = 0; i < PROCESS_MAX_COUNT; i ++){
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

    // clean process
    int pid = p->pid;
    memset(p, 0, sizeof(Process));

    // reinit
    p->state = PROC_UNUSED;
    p->kstack_bottom = KERNEL_STACK(pid);
    p->pid = pid;
    p->is_killed = false;

}

/*
 * 找到可以执行的进程并且上锁
 * 需要调用 spinlock_release 对进程解锁
 */
Process* find_ready_process(){
    pr_debug("find_ready_process");
    int idx = (current_process->pid + 1) % PROCESS_MAX_COUNT;
    for (int i = 0; i < PROCESS_MAX_COUNT; i ++, idx = (idx + 1) % PROCESS_MAX_COUNT){
        if (idx == 0) continue; // 优先跳过空闲进程

        // 尝试获取当前可以运行进程的锁
        // 如果发现进程持有锁（比如正在运行）跳过

        if (process_list[idx].state == PROC_READY){
            return &process_list[idx];
        }

    }

    return &process_list[0];
}

// Give up the CPU for one scheduling round.
void yield(void) {
    pr_debug("yield");
    Process *old = current_process;
    current_process = find_ready_process();
    
    if (old->state == PROC_RUNNING) old->state = PROC_READY;
    current_process->state = PROC_RUNNING;

    // pr_debug("change the process from pid=%d to pid=%d", old->pid, current_process->pid);

    if (old == current_process){
        return;  // 不切换
    }

    switch_ctx(&old->context, &current_process->context);
}

void scheduler(void) {
    pr_debug("scheduler");
    Process *p;

    while (1){
        interrupt_on();
        interrupt_off();
        
        p = find_ready_process();
        if (p == &process_list[0]){
            pr_warn("cpu is idle");
            WAITING_INT;
        }else{
            pr_cont_debug("current_process is "); process_debug(current_process);
            p->state = PROC_RUNNING;
            Process *old = current_process;

            // debug
            pr_cont_debug("switch to the process : "); process_debug(p);

            current_process = p;
            switch_ctx(&old->context, &current_process->context);
        }
    }
}

/* 
 * fork 父亲进程
 * return:
 *      pid of son
 */
i64 kfork(const char * name){
    // TODO:: 上锁确保多进程安全

    Process* fa = current_process;
    Process* p = create_user_process();
    if (p == NULL) return -1;

    p->name = name;

    process_debug(fa);
    process_debug(p);
    
    // copy normal page
    // pr_debug("copy normal page");
    if (copy_page_table(fa->page_table, p->page_table, fa->alloc_page_count) == false){
        free_user_process(p);
        return -1;
    }

    ASSERT(p->alloc_page_count == 0);
    p->alloc_page_count += fa->alloc_page_count;
 
    for (u64 va = PROCESS_STACK_PAGE_START; va < PROCESS_STACK_PAGE_END; va += PAGE_SIZE){
        void* fas = (void *)vaddr_to_paddr(fa->page_table, va);
        void* ps = (void *)vaddr_to_paddr(p->page_table, va);
        memcpy(ps, fas, PAGE_SIZE);
    }
 
    *(p->trap_frame_page) = *(fa->trap_frame_page);
    p->trap_frame_page->a0 = 0; // 子进程 return 0
    p->father = fa;

    return p->pid;
}

Process * get_proc_from_pid(i64 pid){
    if (pid < 0) return NULL;
    return &process_list[pid];
}

void sleep_prepare(void *chan){
    current_process->chan = chan;
}

void sleep(){
    if (current_process->chan) {
        current_process->state = PROC_SLEEP;
        yield();
    }
}

/* 等待一个子进程退出，返回一个u64 */
u64 kwait(u64 exit_number){
    i64 copy_size = sizeof(current_process->exit_number);
    
    while (1){
        bool haschild = false;
        for (int i = 1; i < PROCESS_MAX_COUNT; i ++){
            if (process_list[i].father != current_process) continue;
            haschild = true;
            if (process_list[i].state == PROC_ZOMBIE){
                if (exit_number){
                    if (copy_to_user(
                        current_process->page_table, exit_number, 
                        (void *)(&process_list[i].exit_number), 
                        copy_size
                    ) != copy_size){
                        // 复制失败
                        return -1;
                    };
                }
                free_user_process(&process_list[i]);
                return i;
            }
        }

        // 没有孩子或者已经死了
        if (!haschild || current_process->is_killed){
            return -1;
        }
 
        // 否则睡觉
        sleep_prepare(current_process);
        sleep();
    }
}

void kkill(i64 pid){
    if (pid < 0 || pid >= PROCESS_MAX_COUNT) PANIC("killed out of range");
    Process *p = get_proc_from_pid(pid);
    if (p->state == PROC_SLEEP) p->state = PROC_READY;
    p->is_killed = true;
}

void abandon_child(Process *p){
    Process * first = &process_list[1];
    for (int i = 2; i < PROCESS_MAX_COUNT; i ++){
        if (process_list[i].father == p){
            process_list[i].father = first; // 换一个父
            wakeup(first);
        }
    }
}

/* 退出进程 为父进程设置 exit_number */
void kexit(i64 exit_number){
    // TODO : close file fd 或者别的什么东西
    if (!current_process->father) PANIC("process do not have a father; maybe fisrt_process is ending;");

    abandon_child(current_process);
    wakeup(current_process->father);
    pr_debug("current_process->father pid = %d", current_process->father->pid);

    current_process->exit_number = exit_number;
    current_process->state = PROC_ZOMBIE;

    yield();

    PANIC("ZOMBIE ALIVE !!!");
}

/* 唤醒所有等待chan的进程 */
void wakeup(void *chan){
    for (int i = 0; i < PROCESS_MAX_COUNT; i++){
        Process *p = &process_list[i];
        if (p->chan == chan){
            p->chan = NULL;
            if (p->state == PROC_SLEEP) {
                p->state = PROC_READY;
            }
        }
    }
}

// u64 kkill(Process *proc){
//     if (proc->father == NULL){
//         pr_warn("poweroff!");
//         interrupt_off();
//         WAITING_INT;
//     }
//
//
// }
