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
void process_prepare_return_user();

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
            timer_interrupt();
            break;
    }

    PANIC("kernel_trap_handler: unknow trap");
}

/* 
 * 用户程序异常处理程序
 */ 
u64 user_trap_handler() {
    pr_debug("user_trap_handler start");
    u64 scause = READ_CSR(CSR_SCAUSE);
    u64 stval = READ_CSR(CSR_STVAL);
    u64 user_pc = READ_CSR(CSR_SEPC);
    u64 sstatus = READ_CSR(CSR_SSTATUS);

    pr_debug("user trap for cause = %lx, pc = %lx, status = %lx, stval = %lx", scause, user_pc, sstatus, stval);
    if ((sstatus & SSTATUS_SPP) == 1){ // from kernel 
        PANIC("user_trap_handler : not from supervisor mode");
    }
    
    // 中断向量更新为 kernel_trap_entry
    // 我们已经在内核态了
    u64 trap_entry = (u64)kernel_trap_entry;
    WRITE_CSR(CSR_STVEC, (u64)trap_entry);
    
    Process *p = current_process;
    p->trap_frame_page->epc = READ_CSR(CSR_SEPC); // 更新返回地址
    
    switch (scause){
        case 1: // software interrupt
            break;
        case 5: // scheduler interrupt
            break;
        case 8: // syscall 
            break;
        case 9: // external interrupt
            break;
        case 13: // page error
            break;
        case 15: // page error
            break;
        default:
            PANIC("user_trap_handler : unknow trap");
    }

    process_prepare_return_user();
    u64 satp = MAKE_SATP(p->page_table);

    return satp;
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
    // 更新中断向量寄存器为 user_trap_entry 
    // 马上需要进入用户态了
    u64 trap_entry = TRAMPOLINE_VADDR + ((u64)user_trap_entry - (u64)trampoline);
    WRITE_CSR(CSR_STVEC, trap_entry);

    Process *p = current_process;
    p->trap_frame_page->kernel_hartid= 0; // 单核
 
    // 内核栈和用户栈共享数据
    p->trap_frame_page->sp          = PROCESS_STACK_PAGE_END;
    p->trap_frame_page->kernel_sp   = p->kstack_bottom + PAGE_SIZE;

    p->trap_frame_page->user_trap_function = (u64)user_trap_entry;
    p->trap_frame_page->kernel_satp = READ_CSR(CSR_SATP);

    p->trap_frame_page->epc = USER_VADDR_START;
    interrupt_on();

    WRITE_CSR(CSR_SEPC, p->trap_frame_page->epc);
    CLEAN_CSR(CSR_SSTATUS, SSTATUS_SPP);    // 开启用户中断许可
}

/* 
 * fork_ret 是 context.ra 指向函数
 * 当切换上下文的时候，cpu立刻执行该函数
 */
void fork_ret(void) {
    process_prepare_return_user();
    void (*trap_ret)(u64) = (void *)(
        TRAMPOLINE_VADDR + ((u64)user_trap_ret - (u64)trampoline)
    );

    pr_debug("\n");
    trap_frame_debug(current_process->trap_frame_page);
    pr_debug("\n");


    // 执行 user_trap_ret，保存上下文，进入用户程序
    u64 atp = MAKE_SATP(current_process->page_table);
    pr_debug("satp is 0x%lx", atp);

    trap_ret(atp);
}
