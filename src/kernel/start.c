#include "kernel/start.h"
#include "kernel/lib/printk.h"
#include "kernel/arch/trap.h"

void kernel_trap_init(void){
    trap_init();
}

void kernel_start(void){
    kernel_trap_init();
    pr_info("fuck you\n");
    // test interrupt
    _ASM("unimp");
    while (1);
}

