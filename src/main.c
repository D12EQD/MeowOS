#include "main.h"
#include "kernel.h"
#include "mem.h"

extern char __bss[], __bss_end[], __stack_top[];

int run_main(void){
    WRITE_CSR(stvec, (u32) kernel_entry);
    _ASM("unimp");
    while (1);
    return 0;
}

