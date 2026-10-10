#include "arch/trap.h"
#include "lib/printf.h"

void handle_trap(struct trap_frame *f) {
    u32 scause = READ_CSR(scause);
    u32 stval = READ_CSR(stval);
    u32 user_pc = READ_CSR(sepc);

    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
}

