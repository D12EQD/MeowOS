#include "kernel/arch/trap.h"
#include "kernel/arch/timer.h"

#include "kernel/proc/process.h"

#include "kernel/lib/printk.h"

/*
 * kernel_trap_entry
 * 进入中断的函数，会保护寄存器并且调用 kernel_trap_handler 对中断进行处理
 */
EXTERN_SYMBOL(kernel_trap_entry);
EXTERN_SYMBOL(trampoline); // 蹦床代码，用于页表映射
EXTERN_FUNC(void user_trap_entry(u64));
EXTERN_FUNC(void user_trap_ret(u64));


enum DeviceTrapType{
    DeviceError, // 错误
    DevcieTimer, // 时间中断
};

#define is_timer_interrupt(x) (x == 0x8000000000000005UL)

int dev_interrupt();
void trap_init();

void trap_init(){
    WRITE_CSR(CSR_STVEC, (u64)kernel_trap_entry); // 设置中断向量地址
}

/* 
 * 内核异常处理程序
 */ 
void kernel_trap_handler(){
    u64 scause = READ_CSR(CSR_SCAUSE);
    u64 stval = READ_CSR(CSR_STVAL);
    u64 k_pc = READ_CSR(CSR_SEPC);
    u64 sstatus = READ_CSR(CSR_SSTATUS);

    if ((sstatus & SSTATUS_SPP) == 0){ // from user 
        PANIC("kernel_trap_handler: not from supervisor mode");
    }

    if (is_disable_interrupt(NULL)){ // 如果不允许中断
        PANIC("kernel_trap_handler: interrupts not enabled");
    }
    
    u64 device_from = dev_interrupt();
    pr_debug("kernel trap for cause = %lx, pc = %lx, status = %lx, stval = %lx", scause, k_pc, sstatus, stval);

    switch (device_from) {
        case DevcieTimer:
            pr_debug("timer");
            yield(); // 切换进程
            break;
    }

    PANIC("kernel_trap_handler: unknow trap");
}

/* 
 * 用户程序异常处理程序
 */ 
void user_trap_handler() {
    pr_debug("user_trap_handler start");
    u64 scause = READ_CSR(CSR_SCAUSE);
    u64 stval = READ_CSR(CSR_STVAL);
    u64 user_pc = READ_CSR(CSR_SEPC);
    u64 sstatus = READ_CSR(CSR_SSTATUS);

    pr_debug("user trap for cause = %lx, pc = %lx, status = %lx, stval = %lx", scause, user_pc, sstatus, stval);
    if ((sstatus & SSTATUS_SPP) == 1){ // from kernel 
        PANIC("user_trap_handler : not from supervisor mode");
    }

    WRITE_CSR(CSR_STVEC, (u64)kernel_trap_entry);
    
    Process *p = current_process;
    p->trap_frame_page->epc = READ_CSR(CSR_SEPC);
    
    PANIC("user_trap_handler : unknow trap");
}

/*
 * device interrupt 
 * 返回 enum DeviceTrapType
 */
int dev_interrupt() {
    u64 scause = READ_CSR(CSR_SCAUSE);
    
    if (is_timer_interrupt(scause)){
        timer_interrupt();
        return DevcieTimer;
    }
    return DeviceError;
}


/* 
 * 进程准备返回用户空间需要进行的一系列初始化操作 
 * 调用该操作需要关闭中断，否则可能会出现错误
 */
void process_prepare_return_user(){
    u64 tp_user_entry = TRAMPOLINE_VADDR + ((u64)user_trap_ret - (u64)trampoline);
    WRITE_CSR(CSR_STVEC, tp_user_entry);

    Process *p = current_process;
    p->trap_frame_page->kernel_hartid= 0; // 单核 
    p->trap_frame_page->kernel_sp = p->kstack_bottom;
    p->trap_frame_page->user_trap_function = (u64)user_trap_entry;
    p->trap_frame_page->kernel_satp = READ_CSR(CSR_SATP);

    SET_CSR(CSR_SSTATUS, SSTATUS_SPIE);
    CLEAN_CSR(CSR_SSTATUS, SSTATUS_SPP);
    WRITE_CSR(CSR_SEPC, p->trap_frame_page->epc);
}

/* 
 * fork_ret 是 context.ra 指向函数
 * 当切换上下文的时候，cpu立刻执行该函数
 */
void fork_ret(void) {
    pr_debug("fork_ret start");
    Process* p = current_process;
    // spinlock_release(&p->lock);

    process_prepare_return_user();
    u64 satp = MAKE_SATP(p->page_table);
    u64 temp = TRAMPOLINE_VADDR + ((u64)user_trap_ret - (u64)trampoline);
    void (*user_trap_ret_ptr)(u64) = (void *)temp;

    // 执行 user_trap_ret ，保存上下文
    user_trap_ret_ptr(satp);
}
