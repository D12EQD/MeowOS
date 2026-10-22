#include "kernel/entry.h"
#include "kernel/arch/trap.h"
#include "kernel/start.h"
#include "kernel/lib/string.h"

EXTERN_SYMBOL(__bss);
EXTERN_SYMBOL(__bss_end);
EXTERN_SYMBOL(__stack_top);

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void){
    _ASM(
        "mv s0, a0\n"
        "mv s1, a1\n"
        "mv sp, %[stack_top]\n"
        "addi sp, sp, -8\n"
        "j kernel_start\n"
        :
        : [stack_top] "r" (__stack_top)
    );
}
