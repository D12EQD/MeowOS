#include "trap.h"
#include "timer.h"
#include "sbi.h"
#include "plic.h"
#include "uart.h"

#include "kernel/syscall/syscall.h"
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
    DeviceTimer, // 时间中断
    DiveceOther, // 其他外设中断
};

#define is_timer_interrupt(x)   (( x ) == 0x8000000000000005UL)
#define is_plic_interrupt(x)    (( x ) == 0x8000000000000009L)

static int dev_interrupt();
static void process_prepare_return_user();

void trap_init(){
    WRITE_CSR(CSR_STVEC, (u64)kernel_trap_entry); // 设置中断向量地址
}

/* 
 * 内核异常处理程序
 */ 
void kernel_trap_handler(){
    u64 sstatus = READ_CSR(CSR_SSTATUS);
    u64 scause = READ_CSR(CSR_SCAUSE);

    __old_printk(
        "kernel trap: scause=%lx sepc=%lx stval=%lx\n",
        scause, READ_CSR(CSR_SEPC), READ_CSR(CSR_STVAL)
    );

    if ((sstatus & SSTATUS_SPP) == 0){ // from user 
        PANIC("kernel_trap_handler: not from supervisor mode");
    }

    if (is_disable_interrupt(NULL)){ // 如果不允许中断
        PANIC("kernel_trap_handler: interrupts not enabled");
    }

    if (dev_interrupt() == DeviceError){ // 没有问题，设备中断处理完毕
        PANIC("kernel_trap_handler: unknow trap");
    }

    __old_printk("OK");
    return;

}

/* 
 * 用户程序异常处理程序
 */ 
u64 user_trap_handler() {
    u64 scause = READ_CSR(CSR_SCAUSE);
    u64 sstatus = READ_CSR(CSR_SSTATUS);
    u64 user_pc = READ_CSR(CSR_SEPC);
    u64 stval = READ_CSR(CSR_STVAL);

    // from kernel
    if ((sstatus & SSTATUS_SPP) == 1){ 
        PANIC("user_trap_handler : not from supervisor mode");
    }

    // 中断向量更新为 kernel_trap_entry
    // 我们已经在内核态了
    u64 trap_entry = (u64)kernel_trap_entry;
    WRITE_CSR(CSR_STVEC, (u64)trap_entry);

    current_process->trap_frame_page->epc = READ_CSR(CSR_SEPC); // 更新返回地址

    switch (scause){
        case 1: // software interrupt
            PANIC("user page fault: pc=%lx stval=%lx", user_pc, stval);
            break;
        case 8: // syscall
            // 开启中断，陷入内核态时会自动关中断
            current_process->trap_frame_page->epc += 4;
            interrupt_on(); 
            syscall();
            break;
        case 13: // page_fault
        case 15: // page_fault
            // page_fault_handle();
            PANIC("user page fault: pc=%lx stval=%lx", user_pc, stval);
            break;
        default: // Is a device interrupt ?
            if (dev_interrupt() == DeviceError) {
                PANIC("Unknow error\n");
            }
    }
    
    if (current_process->is_killed){
        kexit(-1);
    }
    
    process_prepare_return_user();
    u64 satp = MAKE_SATP(current_process->page_table);
    
    // return to trampolite.S 的 user_ret
    return satp;
}

/*
 * device interrupt
 * returns:
 *      enum DeviceTrapType, error number in this enum too
 */
int dev_interrupt() {
    u64 scause = READ_CSR(CSR_SCAUSE);
 
    if (is_timer_interrupt(scause)){
        timer_interrupt();
        return DeviceTimer;
    }else if (is_plic_interrupt(scause)){
        int irq = plic_claim();
        __old_printk("plic interrupt start");

        if (irq == UART0_IRQ){
            uart_intr();
        }else{
            // pr_warn("unknow device interrupt");
            __old_printk("unknow device interrupt");
            goto failed;
        }

        __old_printk("plic interrupt end");
        if (irq) plic_complete(irq);
        return DiveceOther;
    }

failed:
    return DeviceError;
}


/* 
 * 进程准备返回用户空间需要进行的一系列初始化操作 
 * 调用该操作需要关闭中断，否则可能会出现错误
 */
void process_prepare_return_user(){
    // 更新中断向量寄存器为 user_trap_entry 需要进入用户态
    // trap_frame_debug(current_process->trap_frame_page);

    u64 trap_entry = TRAMPOLINE_VADDR + ((u64)user_trap_entry - (u64)trampoline);
    WRITE_CSR(CSR_STVEC, trap_entry);

    Process *p = current_process;
    p->trap_frame_page->kernel_hartid= 0; // 单核
 
    // 内核栈和用户栈共享数据
    p->trap_frame_page->kernel_sp = p->kstack_bottom + PAGE_SIZE;

    p->trap_frame_page->user_trap_function = (u64)user_trap_handler;
    p->trap_frame_page->kernel_satp = READ_CSR(CSR_SATP);
    
    WRITE_CSR(CSR_SEPC, p->trap_frame_page->epc);
    CLEAN_CSR(CSR_SSTATUS, SSTATUS_SPP);  // 开启用户中断许可

}

/* 
 * fork_ret 是 context.ra 指向函数
 * 当切换上下文的时候，cpu立刻执行该函数
 */
void fork_ret(void) {
    interrupt_off();

    void (*trap_ret)(u64) = (void *)(
        TRAMPOLINE_VADDR + ((u64)user_trap_ret - (u64)trampoline)
    );

    process_prepare_return_user();
    u64 atp = MAKE_SATP(current_process->page_table);

    interrupt_on();

    trap_ret(atp);
}
