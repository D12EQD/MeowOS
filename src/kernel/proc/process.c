#include "kernel/proc/process.h"
#include "kernel/proc/lock.h"

#include "kernel/arch/trap.h"

#include "kernel/arch/def.h"
#include "kernel/arch/trap.h"

#include "kernel/lib/string.h"
#include "kernel/lib/def.h"
#include "kernel/lib/printk.h"

#include "kernel/mm/def.h"
#include "kernel/mm/page.h"
#include "kernel/mm/pmm.h"

#include "kernel/fs/io.h"
#include "kernel/fs/fs.h"
#include "kernel/fs/file.h"

EXTERN_SYMBOL(trampoline); // 蹦床代码，用于页表映射
EXTERN_SYMBOL(_binary_user_bin_start);
EXTERN_SYMBOL(_binary_user_bin_end);

EXTERN_FUNC(void switch_ctx(ThreadCtx *old, ThreadCtx* new));
EXTERN_FUNC(void user_trap_entry(void *));
EXTERN_FUNC(void user_trap_ret(void *));

// 堆顶：下一个可以分配的页面的虚拟地址
#define PROC_TOP_VADDR(page_count) (UESR_HEAP_START + (page_count) * PAGE_SIZE)

Process process_list[PROCESS_MAX_COUNT];
Process *current_process;

static inline void process_free_page_table(Process* p){
    critical_enter();
    user_unmap_pages(p->page_table, TRAPFRAME_VADDR, 1, true);
    user_unmap_pages(p->page_table, TRAMPOLINE_VADDR, 1, false);

    user_unmap_pages(p->page_table, UESR_DATA_PAGE_START, USER_DATA_PAGE_COUNT, true);
    user_unmap_pages(p->page_table, USER_CODE_START, USER_CODE_PAGE_COUNT, true);
    user_unmap_pages(p->page_table, UESR_HEAP_START, p->alloc_page_count, true);

    user_unmap_pages(p->page_table, PROCESS_STACK_PAGE_START, PROCESS_STACK_PAGE_COUNT, true);
    free_walk(p->page_table, 2);
    critical_exit();
}

void process_display(Process *p){
    pr_info("[");
    if (p->name) pr_info("  process name : %s", p->name);
    pr_info("  process_info : pid = %d", p->pid);
    pr_info("  context infomation : ra = %lx, sp = %lx", 
        p->context.ra, p->context.sp);
    pr_info("  page_table = %lx, frame_page = %lx", 
        (u64)p->page_table, (u64)p->trap_frame_page
    );
    pr_info("]\n");
}

void trap_frame_display(TrapFrame *tf){
     if (!tf) {
        pr_cont_info("TrapFrame: NULL\n");
        return;
    }

    pr_cont_info("TrapFrame @ 0x%lx {\n", (u64)tf);
    pr_cont_info("  [  0] kernel_satp        = 0x%lx\n", tf->kernel_satp);
    pr_cont_info("  [  8] kernel_sp          = 0x%lx\n", tf->kernel_sp);
    pr_cont_info("  [ 16] user_trap_function = 0x%lx\n", tf->user_trap_function);
    pr_cont_info("  [ 24] epc                = 0x%lx\n", tf->epc);
    pr_cont_info("  [ 32] kernel_hartid      = 0x%lx\n", tf->kernel_hartid);

    pr_cont_info("  [ 40] ra                 = 0x%lx\n", tf->ra);
    pr_cont_info("  [ 48] sp                 = 0x%lx\n", tf->sp);
    pr_cont_info("  [ 56] gp                 = 0x%lx\n", tf->gp);
    pr_cont_info("  [ 64] tp                 = 0x%lx\n", tf->tp);
    pr_cont_info("  [ 72] t0                 = 0x%lx\n", tf->t0);
    pr_cont_info("  [ 80] t1                 = 0x%lx\n", tf->t1);
    pr_cont_info("  [ 88] t2                 = 0x%lx\n", tf->t2);
    pr_cont_info("  [ 96] s0                 = 0x%lx\n", tf->s0);
    pr_cont_info("  [104] s1                 = 0x%lx\n", tf->s1);

    pr_cont_info("  [112] a0                 = 0x%lx\n", tf->a0);
    pr_cont_info("  [120] a1                 = 0x%lx\n", tf->a1);
    pr_cont_info("  [128] a2                 = 0x%lx\n", tf->a2);
    pr_cont_info("  [136] a3                 = 0x%lx\n", tf->a3);
    pr_cont_info("  [144] a4                 = 0x%lx\n", tf->a4);
    pr_cont_info("  [152] a5                 = 0x%lx\n", tf->a5);
    pr_cont_info("  [160] a6                 = 0x%lx\n", tf->a6);
    pr_cont_info("  [168] a7                 = 0x%lx\n", tf->a7);

    pr_cont_info("  [176] s2                 = 0x%lx\n", tf->s2);
    pr_cont_info("  [184] s3                 = 0x%lx\n", tf->s3);
    pr_cont_info("  [192] s4                 = 0x%lx\n", tf->s4);
    pr_cont_info("  [200] s5                 = 0x%lx\n", tf->s5);
    pr_cont_info("  [208] s6                 = 0x%lx\n", tf->s6);
    pr_cont_info("  [216] s7                 = 0x%lx\n", tf->s7);
    pr_cont_info("  [224] s8                 = 0x%lx\n", tf->s8);
    pr_cont_info("  [232] s9                 = 0x%lx\n", tf->s9);
    pr_cont_info("  [240] s10                = 0x%lx\n", tf->s10);
    pr_cont_info("  [248] s11                = 0x%lx\n", tf->s11);

    pr_cont_info("  [256] t3                 = 0x%lx\n", tf->t3);
    pr_cont_info("  [264] t4                 = 0x%lx\n", tf->t4);
    pr_cont_info("  [272] t5                 = 0x%lx\n", tf->t5);
    pr_cont_info("  [280] t6                 = 0x%lx\n", tf->t6);
    pr_cont_info("}\n");
}

void create_idle_process(){
    // 0 号为空闲进程 纯占位置
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
        memset(&process_list[i].file_table, 0, PROCESS_MAX_FD_COUNT * sizeof(File *));
    }
    create_idle_process();
}

/* 创建用户进程页表 */
pt_t create_user_process_page(Process *p){
    critical_enter();
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

    // 3. 用户栈 放在高位，不算入 alloc_page_count
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
    
    critical_exit();
    return page_table;

stack_free:
    user_unmap_pages(page_table, PROCESS_STACK_PAGE_START, PROCESS_STACK_PAGE_COUNT, true);
// trapframe_free:
    user_unmap_pages(page_table, TRAPFRAME_VADDR, 1, true);
trampoline_free:
    user_unmap_pages(page_table, TRAMPOLINE_VADDR, 1, false);
page_table_free:
    if (page_table) free_walk(page_table, 2);
    critical_exit();

    return NULL;
}

/*
 * 把用户镜像中一段连续内容逐页拷进目标页表映射到的物理页
 * src 与 dst_va 一一对应：
 *      src[0] -> dst_va
 *      src[n] -> dst_va + n
 */
static bool copy_image_to_user(pt_t page_table, u64 dst_va, const void *src, u64 len){
    const u8 *s = (const u8 *)src;
    u64 done = 0;

    while (done < len){
        u64 n = len - done;
        if (n > PAGE_SIZE) n = PAGE_SIZE;

        u64 dst_pa = vaddr_to_paddr(page_table, dst_va + done);
        if unlikely (!dst_pa) return false;
        if unlikely (memcpy((void *)dst_pa, s + done, n) != n) return false;

        done += n;
    }

    return true;
}

/* 
 * 初始化第一个用户进程
 *
 * 用户地址空间布局（由 user.ld 固定，宏见 mm/def.h）：
 *      [0x0000, 0x1000) 未映射，用作 NULL 陷阱页
 *      [0x1000, 0x4000) 静态数据 .data / .bss，可读写不可执行
 *      [0x4000,  x+1  ) 代码 .text / .rodata，可执行不可写
 *      [x + 1,  x + 2 ) 未映射，guard 页
 *      [x + 2,  ...   ) 堆，由 kpalloc 分配
 */
bool first_user_init(Process *proc){
    ASSERT(proc->pid == 1);
    ASSERT(proc->page_table);
    pt_t page_table = proc->page_table;

    const u8 *bin = (const u8 *)_binary_user_bin_start;
    u64 bin_size = USER_IMAGE_SIZE;

    // 校验镜像
    u32 magic = 0;
    memcpy(&magic, bin, sizeof(magic));
    ASSERT(magic == USER_IMAGE_MAGIC);

    // 镜像至少要覆盖数据区
    ASSERT(bin_size > USER_DATA_PAGE_COUNT * PAGE_SIZE);

    // 1. 数据页：可读可写，不可执行
    if (user_map_pages(page_table, UESR_DATA_PAGE_START, USER_DATA_PAGE_COUNT, PTE_R | PTE_W)
            != USER_DATA_PAGE_COUNT){
        goto error;
    }

    // 2. 代码页：从 0x4000 起，可读可执行，不可写
    if (user_map_pages(page_table, USER_CODE_START, USER_CODE_PAGE_COUNT, PTE_R | PTE_X)
            != USER_CODE_PAGE_COUNT){
        goto error;
    }

    // 3. 整段拷贝：数据页和代码页在镜像里连续，一次拷完
    // （x + 1 的 guard 页在这里没有被映射，堆起点 UESR_HEAP_START 自然落在它后面）
    if (!copy_image_to_user(page_table, UESR_VADDR_START, bin, bin_size)){
        goto error;
    }

    // 4. create stdin stdout
    
    proc->file_table[STDIN] = open_file(
        sv_lit(IO_STDIN), O_CREATE | O_READ, &io_fop, INODE_IO
    );

    proc->file_table[STDOUT] = open_file(
        sv_lit(IO_STDOUT), O_CREATE | O_WRITE, &io_fop, INODE_IO
    );
    ASSERT(proc->file_table[STDOUT] && proc->file_table[STDOUT]);
 
    ASSERT(vaddr_to_paddr(page_table, USER_GUARD_VADDR) == 0);
    ASSERT(UESR_HEAP_START == USER_GUARD_VADDR + PAGE_SIZE);

    proc->trap_frame_page->epc = USER_CODE_START;
    proc->alloc_page_count = 0;
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
    critical_enter();
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

    critical_exit();
    return p;

free_trap_frame_page:
    kfree(p->trap_frame_page);
return_null:
    critical_exit();

    return NULL;
}

/*
 * 释放一个用户进程
 * 该操作会释放该用户分配的页表 和 unref 打开文件表中的文件
 */
void free_user_process(Process* p){
    critical_enter();

    p->state = PROC_UNUSED;
    process_free_page_table(p);

    // clean process
    int pid = p->pid;
    memset(p, 0, sizeof(Process));

    p->state = PROC_UNUSED;
    p->kstack_bottom = KERNEL_STACK(pid);
    p->pid = pid;
    

    for (int i = 0; i < PROCESS_MAX_FD_COUNT; i ++){
        File* file = p->file_table[i];
        if (file){
            unref_file(file);
        }
    }

    critical_exit();
}

/*
 * 找到可以执行的进程并且上锁
 * 需要调用 spinlock_release 对进程解锁
 */
Process* find_ready_process(){
    critical_enter();

    int idx = (current_process->pid + 1) % PROCESS_MAX_COUNT;
    for (int i = 0; i < PROCESS_MAX_COUNT; i ++, idx = (idx + 1) % PROCESS_MAX_COUNT){
        if (idx == 0) continue; // 优先跳过空闲进程

        if (process_list[idx].state == PROC_READY){
            critical_exit();
            return &process_list[idx];
        }

    }

    critical_exit();

    return &process_list[0];
}

// Give up the CPU for one scheduling round.
void yield(void) {
    Process *old = current_process;
    current_process = find_ready_process();

    if (old->state == PROC_RUNNING) old->state = PROC_READY;
    current_process->state = PROC_RUNNING;

    if (old == current_process){
        return;  // 不切换
    }

    switch_ctx(&old->context, &current_process->context);
}

void scheduler(void) {
    Process *p;

    critical_enter();
    while (1){
        critical_exit();
        critical_enter();

        p = find_ready_process();
        if (p == &process_list[0]){
            pr_warn("cpu is idle");
            WAITING_INT;
        }else{
            p->state = PROC_RUNNING;
            Process *old = current_process;
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
    Process* fa = current_process;
    Process* p = create_user_process();
    if (p == NULL) return -1;

    p->name = name;

    // 1. data
    if unlikely (copy_page_table(
            fa->page_table, p->page_table, UESR_VADDR_START, USER_DATA_PAGE_COUNT) == false){
        goto copy_page_table_failed;
    }

    // 2. code
    if unlikely (copy_page_table(
            fa->page_table, p->page_table, USER_CODE_START, USER_CODE_PAGE_COUNT) == false){
        goto copy_page_table_failed;
    }

    // 3. heap
    if unlikely (copy_page_table(
            fa->page_table, p->page_table, UESR_HEAP_START, fa->alloc_page_count) == false){
        goto copy_page_table_failed;
    }

    ASSERT(p->alloc_page_count == 0);
    p->alloc_page_count = fa->alloc_page_count;
 
    for (u64 va = PROCESS_STACK_PAGE_START; va < PROCESS_STACK_PAGE_END; va += PAGE_SIZE){
        void* fas = (void *)vaddr_to_paddr(fa->page_table, va);
        void* ps = (void *)vaddr_to_paddr(p->page_table, va);

        if unlikely (!fas || !ps) goto memcpy_failed;

        // memcpy 返回 copy size
        if unlikely (memcpy(ps, fas, PAGE_SIZE) != PAGE_SIZE) goto memcpy_failed;
    }

    // TODO : fork 时文件表复制指针并 f_ref ++，父子共享 offset
    for (int i = 0; i < FS_MAX_FILE_COUNT; i ++){
 
    }

    *(p->trap_frame_page) = *(fa->trap_frame_page);
    p->trap_frame_page->a0 = 0; // 子进程 return 0
    p->father = fa;

    return p->pid;

memcpy_failed:
copy_page_table_failed:
    free_user_process(p);
    return -1;
}

Process * get_proc_from_pid(i64 pid){
    if (pid < 0) return NULL;
    return &process_list[pid];
}

void sleep_prepare(void *chan){
    critical_enter();
    current_process->chan = chan;
    critical_exit();
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

    while (1) {
        bool haschild = false;

        for (int i = 1; i < PROCESS_MAX_COUNT; i++) {
            critical_enter();
            bool is_child  = (process_list[i].father == current_process);
            bool is_zombie = is_child && (process_list[i].state == PROC_ZOMBIE);
            critical_exit();

            if (!is_child) continue;
            haschild = true;

            if (is_zombie) {

                if (exit_number &&
                    copy_to_user(current_process->page_table, (void *)exit_number, (void *)&process_list[i].exit_number, copy_size
                ) != copy_size) {
                    current_process->chan = NULL;
                    return -1;
                }

                free_user_process(&process_list[i]);
                current_process->chan = NULL;
                return i;
            }
        }

        if (!haschild || current_process->is_killed) {
            return -1;
        }

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
    if (!current_process->father) PANIC("process do not have a father; maybe fisrt_process is ending;");

    abandon_child(current_process);
    wakeup(current_process->father);

    current_process->exit_number = exit_number;
    current_process->state = PROC_ZOMBIE;

    yield();

    PANIC("ZOMBIE ALIVE !!!");
}

/* 
 * 唤醒所有等待chan的进程 
 * void * chan 可以是任意的东西，变量，进程都可以
 */
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


/* 
 * 分配进程分配的连续内存 
 * returns: 
 *      success : void* address 分配开始的地址
 *      failed : 0
 */
u64 kpalloc(u64 page_count){
    if (page_count == 0) return 0;
    u64 addr = PROC_TOP_VADDR(current_process->alloc_page_count);

    u64 ret = user_map_pages(
        current_process->page_table, 
        addr,
        page_count,
        PTE_W | PTE_R
    );
    current_process->alloc_page_count += ret;

    if (ret != page_count){
        kpfree(ret); // 其中会 current_process->alloc_page_count -= ret;
        return 0;
    }

    return addr;
}

/*
 * 释放进程堆区最后 page_count 页（栈式释放，必须与 kpalloc 顺序匹配）
 */
void kpfree(u64 page_count){
    if (page_count == 0) return;
    if (page_count > current_process->alloc_page_count) goto failed;


    user_unmap_pages(
        current_process->page_table,
        PROC_TOP_VADDR(current_process->alloc_page_count - page_count),
        page_count, true
    );

    current_process->alloc_page_count -= page_count;
    return;

failed:
    current_process->is_killed = true; // 杀
    current_process->exit_number = -1;
    return;
}

