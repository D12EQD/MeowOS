#include "kernel/arch/trap.h"
#include "kernel/arch/timer.h"

#include "kernel/proc/process.h"

#include "kernel/lib/printk.h"

EXTERN_SYMBOL(kernel_trap_entry);

enum DeviceTrapType{
    DeviceError, // 错误
    DevcieTimer, // 时间中断
};

#define is_timer_interrupt(x) (x == 0x8000000000000005UL)

int dev_interrupt();
bool is_interrupt();
void trap_init();

void trap_init(){
    WRITE_CSR(CSR_STVEC, (u64)kernel_trap_entry); // 设置中断向量地址
    interrupt_on();
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

    if (is_interrupt() == false){ // 如果不允许中断
        PANIC("kernel_trap_handler: interrupts not enabled");
    }
    
    u64 device_from = dev_interrupt();
    prln_debug("kernel trap for cause = %lx, pc = %lx, status = %lx", scause, k_pc, sstatus);

    switch (device_from) {
        case DevcieTimer:
            prln_debug("timer");
            yield(); // 切换进程
            break;
        default:
            PANIC("kernel_trap_handler: unknow");
    }
}

/* 
 * 用户程序异常处理程序
 */ 
void user_trap_handler() {
    u64 scause = READ_CSR(CSR_SCAUSE);
    u64 stval = READ_CSR(CSR_STVAL);
    u64 user_pc = READ_CSR(CSR_SEPC);
    u64 sstatus = READ_CSR(CSR_SSTATUS);

    if ((sstatus & SSTATUS_SPP) == 1){ // from kernel 
        PANIC("user_trap_handler : not from supervisor mode");
    }

    WRITE_CSR(CSR_STVEC, (u64)kernel_trap_entry);
    prln_debug("kernel trap for cause = %lx, pc = %lx, status = %lx", scause, user_pc, sstatus);
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
