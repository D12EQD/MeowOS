#pragma once
#include "lib/printf.h"
#include "lib/def.h"

#define READ_CSR(reg)                               \
    ({                                              \
        u64 __tmp;                                  \
        _ASM("csrr %0, " #reg : "=r"(__tmp));       \
        __tmp;                                      \
    })

#define WRITE_CSR(reg, value)                       \
    do {                                            \
        u64 __tmp = (value);                        \
        _ASM("csrw " #reg ", %0" ::"r"(__tmp));     \
    } while (0)

struct trap_frame {
    u64 ra, gp, tp;
    u64 t0, t1, t2, t3, t4, t5, t6;
    u64 a0, a1, a2, a3, a4, a5, a6, a7;
    u64 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    u64 sp;  // 原始 sp（来自 sscratch）
}__attribute__((packed));

void handle_trap(struct trap_frame *f);





