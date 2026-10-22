#include "kernel/arch/trap.h"
#include "kernel/arch/def.h"
#include "kernel/proc/lock.h"

u64 critical_enter(){
    u64 old = interrupt_off() & SSTATUS_SIE;
    return old;
}

u64 critical_exit(){
    u64 old = interrupt_on() & SSTATUS_SIE;
    return old;
}
