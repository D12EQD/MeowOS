#pragma once
#include "kernel/lib/printk.h"
#include "kernel/lib/def.h"
#include "kernel/arch/def.h"

u64 user_trap_handler();
void kernel_trap_handler();
void trap_init();
void fork_ret(void);

// enable device interrupts and return the old value
static inline u64 interrupt_on() {
    u64 res = READ_SET_CSR(CSR_SSTATUS, SSTATUS_SIE);
    return res;
}

// disable device interrupts and return the old value
static inline u64 interrupt_off() {
    u64 res = READ_CLEAN_CSR(CSR_SSTATUS, SSTATUS_SIE);
    return res;
}

// 当前 sstatus.SIE 是否为 1，即中断是否开着
static inline bool is_enable_interrupt(u64 *ret){
    u64 x = READ_CSR(CSR_SSTATUS);
    if (ret) *ret = x;
    return (x & SSTATUS_SIE) != 0;
}

// 当前 sstatus.SIE 是否为 0，即中断是否关着
static inline bool is_disable_interrupt(u64 *ret){
    u64 x = READ_CSR(CSR_SSTATUS);
    if (ret) *ret = x;
    return (x & SSTATUS_SIE) == 0;
}
