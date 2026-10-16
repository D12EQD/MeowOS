#include "kernel/arch/trap.h"
#include "kernel/lib/printk.h"

EXTERN_SYMBOL(kernel_trap_entry);

// enable device interrupts
static inline void interrupt_on() {
    SET_CSR(CSR_SSTATUS, SSTATUS_SIE);
}

// disable device interrupts
static inline void interrupt_off() {
    CLEAN_CSR(CSR_SSTATUS, SSTATUS_SIE);
}

bool is_interrupt(){
    u64 x = READ_CSR(CSR_SSTATUS);
    return (x & SSTATUS_SIE) == 0;
}

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
        PANIC("kernel_trap_handler: not from supervisor mode\n");
    }

    if (is_interrupt() == false){ // 如果不允许中断
        PANIC("kernel_trap_handler: interrupts not enabled\n");
    }    

    PANIC("trap stop , test is ok\n");
}

/* 
 * 用户程序异常处理程序
 */ 
void user_trap_handler(struct trap_frame *f) {
    u64 scause = READ_CSR(CSR_SCAUSE);
    u64 stval = READ_CSR(CSR_STVAL);
    u64 user_pc = READ_CSR(CSR_SEPC);
    u64 sstatus = READ_CSR(CSR_SSTATUS);

    if ((sstatus & SSTATUS_SPP) == 1){ // from kernel 
        PANIC("user_trap_handler : not from supervisor mode\n");
    }

    // WRITE_CSR(stvec, (u64)kernel_trap_entry);

    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
}

/* 
 * check if it's an external interrupt or software interrupt, and handle it.
 * returns :
 *      2 if timer interrupt,
 *      1 if other device,
 *      0 if not recognized.
 */
int dev_interrupt() {
    u64 scause = READ_CSR(CSR_SCAUSE);

    if (scause == 0x8000000000000005L) {
        // clock_interrupt();
        return 2;
    }
    // TODO : Other interrupt

    return 0;
}
