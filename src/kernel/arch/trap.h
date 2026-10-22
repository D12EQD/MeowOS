#pragma once
#include "kernel/lib/printk.h"
#include "kernel/lib/def.h"
#include "kernel/arch/def.h"

void user_trap_handler();
void kernel_trap_handler();
void trap_init();

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

static inline bool is_interrupt(){
    u64 x = READ_CSR(CSR_SSTATUS);
    return (x & SSTATUS_SIE) == 0;
}

