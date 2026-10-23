#include "kernel/arch/trap.h"
#include "kernel/arch/def.h"

#include "kernel/proc/lock.h"
#include "kernel/proc/process.h" 

void critical_enter(){
    u64 old = interrupt_off() & SSTATUS_SIE;
    Cpu * cpu = get_cpu();
    if (old){ // 首次关中断
        cpu->old_sie = old;
    }
    cpu->interrput_depth ++;
}

void critical_exit(){
    u64 sie = 0;
    if (is_disable_interrupt(&sie)){
        PANIC("critical_exit error : is_disable_interrupt");
    }
    Cpu * cpu = get_cpu();
    cpu->interrput_depth --;
    if (cpu->interrput_depth < 0){
        PANIC("critical_exit error : interrput_depth");
    }
    if (cpu->interrput_depth == 0 && cpu->old_sie){
        cpu->old_sie = 0;
        interrupt_on();
    }
}

void spin_lock()
