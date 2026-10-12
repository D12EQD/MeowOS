#include "kernel/kernel.h"
#include "kernel/process.h"

#include "lib/printf.h"
#include "lib/string.h"

#include "arch/trap.h"
#include "arch/sbi.h"

#include "mm/pmm.h"

extern char __bss[], __bss_end[], __stack_top[];

struct process *proc_a;
struct process *proc_b;

void kernel_entry(void);
void kernel_main(void);
void delay();

void delay(){
    for (int i = 0; i < 70000000; i++) _ASM("nop");
}

void proc_a_entry(void) {;
    while (1) {
        putchar('A');
        delay();
        yield();
    }
}

void proc_b_entry(void) {
    while (1) {
        putchar('B');
        delay();
        yield();
    }
}

void kernel_main(void){
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss); 
    printf("fuck\n");
    WRITE_CSR(
        stvec, 
        (paddr_t) kernel_entry
    );
    process_init();
    proc_a = create_process((paddr_t) proc_a_entry);
    proc_b = create_process((paddr_t) proc_b_entry);

    printf("the addr of process a is %p | state is %d pid is | %d\n", proc_a, proc_a->state, proc_a->pid);
    printf("the addr of process b is %p | state is %d pid is | %d\n", proc_b, proc_b->state, proc_b->pid);

    printf("start processing\n");
    yield();
}


/* 
 * 异常处理程序的入口点
 * 保存上下文并且调用 handle_trap 处理具体程序
 */
__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    _ASM(
        "csrw sscratch, sp\n"
        "addi sp, sp, -8 * 31\n"
        "sw ra,  8 * 0(sp)\n"
        "sw gp,  8 * 1(sp)\n"
        "sw tp,  8 * 2(sp)\n"
        "sw t0,  8 * 3(sp)\n"
        "sw t1,  8 * 4(sp)\n"
        "sw t2,  8 * 5(sp)\n"
        "sw t3,  8 * 6(sp)\n"
        "sw t4,  8 * 7(sp)\n"
        "sw t5,  8 * 8(sp)\n"
        "sw t6,  8 * 9(sp)\n"
        "sw a0,  8 * 10(sp)\n"
        "sw a1,  8 * 11(sp)\n"
        "sw a2,  8 * 12(sp)\n"
        "sw a3,  8 * 13(sp)\n"
        "sw a4,  8 * 14(sp)\n"
        "sw a5,  8 * 15(sp)\n"
        "sw a6,  8 * 16(sp)\n"
        "sw a7,  8 * 17(sp)\n"
        "sw s0,  8 * 18(sp)\n"
        "sw s1,  8 * 19(sp)\n"
        "sw s2,  8 * 20(sp)\n"
        "sw s3,  8 * 21(sp)\n"
        "sw s4,  8 * 22(sp)\n"
        "sw s5,  8 * 23(sp)\n"
        "sw s6,  8 * 24(sp)\n"
        "sw s7,  8 * 25(sp)\n"
        "sw s8,  8 * 26(sp)\n"
        "sw s9,  8 * 27(sp)\n"
        "sw s10, 8 * 28(sp)\n"
        "sw s11, 8 * 29(sp)\n"

        "csrr a0, sscratch\n"
        "sw a0, 8 * 30(sp)\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  8 * 0(sp)\n"
        "lw gp,  8 * 1(sp)\n"
        "lw tp,  8 * 2(sp)\n"
        "lw t0,  8 * 3(sp)\n"
        "lw t1,  8 * 4(sp)\n"
        "lw t2,  8 * 5(sp)\n"
        "lw t3,  8 * 6(sp)\n"
        "lw t4,  8 * 7(sp)\n"
        "lw t5,  8 * 8(sp)\n"
        "lw t6,  8 * 9(sp)\n"
        "lw a0,  8 * 10(sp)\n"
        "lw a1,  8 * 11(sp)\n"
        "lw a2,  8 * 12(sp)\n"
        "lw a3,  8 * 13(sp)\n"
        "lw a4,  8 * 14(sp)\n"
        "lw a5,  8 * 15(sp)\n"
        "lw a6,  8 * 16(sp)\n"
        "lw a7,  8 * 17(sp)\n"
        "lw s0,  8 * 18(sp)\n"
        "lw s1,  8 * 19(sp)\n"
        "lw s2,  8 * 20(sp)\n"
        "lw s3,  8 * 21(sp)\n"
        "lw s4,  8 * 22(sp)\n"
        "lw s5,  8 * 23(sp)\n"
        "lw s6,  8 * 24(sp)\n"
        "lw s7,  8 * 25(sp)\n"
        "lw s8,  8 * 26(sp)\n"
        "lw s9,  8 * 27(sp)\n"
        "lw s10, 8 * 28(sp)\n"
        "lw s11, 8 * 29(sp)\n"
        "lw sp,  8 * 30(sp)\n"
        "sret\n"
    );
}

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void){
    _ASM(
        "mv sp, %[stack_top]\n"
        "j kernel_main\n"
        :
        : [stack_top] "r" (__stack_top)
    );
}
