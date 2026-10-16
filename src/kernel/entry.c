#include "kernel/entry.h"
#include "kernel/arch/trap.h"
#include "kernel/start.h"
#include "kernel/lib/string.h"

EXTERN_SYMBOL(__bss);
EXTERN_SYMBOL(__bss_end);
EXTERN_SYMBOL(__stack_top);

void kernel_init(void){
    memset(__bss, 0, (u64) __bss_end - (u64) __bss);  // bss初始化为0
    trap_init();
}

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void){
    _ASM(
        "mv sp, %[stack_top]\n"
        "j kernel_start\n"
        :
        : [stack_top] "r" (__stack_top)
    );
}
