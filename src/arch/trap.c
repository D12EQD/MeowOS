#include "arch/trap.h"
#include "lib/printf.h"

/* 
 * 异常处理程序，实现异常分发和判断 
 *  TODO : implement trap function
 */ 
void handle_trap(struct trap_frame *f) {
    u64 scause = READ_CSR(scause);
    u64 stval = READ_CSR(stval);
    u64 user_pc = READ_CSR(sepc);

    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
}

