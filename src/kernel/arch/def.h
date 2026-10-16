#pragma once
/* 
 * 在这里定义重要的架构定义 
 * 1. 针对链接工具进行对接 包括页表定义和高地址内存所需要的页表
 * 2. 特殊寄存器定义
 */

#define MAX_VADDR (1 << 39)
#define PAGE_SIZE 4096
#define TRAMPOLINE_VADDR (MAX_VADDR - PAGE_SIZE) // trampoline 帧最高
#define TRAPFRAME_VADDR (TRAMPOLINE_VADDR - PAGE_SIZE) // 陷入帧第二高

#define SSTATUS_SPP  (1L << 8) // Previous mode, 1 = Supervisor, 0 = User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE  (1L << 1) // Supervisor Interrupt Enable
#define SSTATUS_UIE  (1L << 0) // User Interrupt Enable

/* static global function */
#define EXTERN_SYMBOL(s) extern char s[];


/* csr function and define */

#define CSR_SCAUSE   "scause"
#define CSR_SEPC     "sepc"
#define CSR_SSTATUS  "sstatus"
#define CSR_STVAL    "stval"
#define CSR_STVEC    "stvec"

// 读取 csr 寄存器
#define READ_CSR(reg)                               \
    ({                                              \
        u64 __tmp;                                  \
        _ASM("csrr %0, " reg : "=r"(__tmp));       \
        __tmp;                                      \
    })

// 写 csr 寄存器
#define WRITE_CSR(reg, value)                       \
    do {                                            \
        u64 __tmp = (value);                        \
        _ASM("csrw " reg ", %0" ::"r"(__tmp));     \
    } while (0)

// set bit csr 寄存器
#define SET_CSR(reg, bits)                          \
    do {                                            \
        u64 __tmp = (bits);                         \
        _ASM("csrs " reg ", %0"                     \
            :: "rK"(__tmp)                          \
            : "memory"                              \
        );                                          \
    } while (0)

// clean bit csr 寄存器
#define CLEAN_CSR(reg, bits)                        \
    do {                                            \
        u64 __tmp = (bits);                         \
        _ASM("csrc " reg ", %0"                    \
            :: "rK"(__tmp)                          \
            : "memory"                              \
        );                                          \
    } while (0)
