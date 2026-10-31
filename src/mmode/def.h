#pragma once

/* 发生异常之前的权限模式保留在 mstatus 的 MPP 域中 */
#define PRV_U 0
#define PRV_S 1
#define PRV_M 3

#define NULL ((void*)(0))

/* 
 * 在这里定义重要的 通用的 架构定义 
 * 1. 针对链接工具进行对接 包括页表定义和高地址内存所需要的页表
 * 2. 特殊寄存器定义
 */
#define MSTATUS_SPP  (1L << 8) // Previous mode, 1 = Supervisor, 0 = User
#define MSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define MSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define MSTATUS_SIE  (1L << 1) // Supervisor Interrupt Enable
#define MSTATUS_UIE  (1L << 0) // User Interrupt Enable

/* static global function */
#define EXTERN_SYMBOL(s) extern char s[];
#define EXTERN_FUNC(func) func

/* cpu 核心数量 */
#define CPU_NUMBER 1

/* csr function and define */
#define CSR_MCAUSE      "mcause"
#define CSR_MEPC        "mepc"
#define CSR_MSTATUS     "mstatus"
#define CSR_MTVAL       "mtval"
#define CSR_MTVEC       "mtvec"
#define CSR_MIE         "mie"
#define CSR_MSCRATCH    "mscratch"

#ifndef _ASM
#define _ASM __asm__ __volatile__
#endif

// 读取通用寄存器
#define READ_REG(reg)                                   \
    ({                                                  \
        long long __tmp;                                \
        _ASM("mv %0, " #reg : "=r"(__tmp));             \
        __tmp;                                          \
    })

// 写通用寄存器
#define WRITE_REG(reg, value)                           \
    do {                                                \
        long long __tmp = (long long)(value);           \
        _ASM("mv " #reg ", %0" ::"r"(__tmp));           \
    } while (0)

// 读取 csr 寄存器
#define READ_CSR(csr_reg)                               \
    ({                                                  \
        long long __tmp;                                \
        _ASM("csrr %0, " csr_reg : "=r"(__tmp));        \
        __tmp;                                          \
    })

// 写 csr 寄存器
#define WRITE_CSR(csr_reg, value)                       \
    do {                                                \
        long long __tmp = (long long)(value);           \
        _ASM("csrw " csr_reg ", %0" ::"r"(__tmp));      \
    } while (0)

// set bit csr 寄存器
#define SET_CSR(csr_reg, bits)                          \
    do {                                                \
        long long __tmp = (long long)(bits);            \
        _ASM("csrs " csr_reg ", %0"                     \
            :: "rK"(__tmp)                              \
            : "memory"                                  \
        );                                              \
    } while (0)

// clean bit csr 寄存器
#define CLEAN_CSR(csr_reg, bits)                        \
    do {                                                \
        long long __tmp = (long long)(bits);            \
        _ASM("csrc " csr_reg ", %0"                     \
            :: "rK"(__tmp)                              \
            : "memory"                                  \
        );                                              \
    } while (0)

// read and clean csr 
#define READ_CLEAN_CSR(csr_reg, val)                    \
    ({                                                  \
        long long __tmp = val;                          \
        _ASM("csrrc %0, " csr_reg ", %1"                \
            : "=r"(__tmp)                               \
            : "rK"(__tmp)                               \
            : "memory");                                \
        __tmp;                                          \
    })

// read and set csr
#define READ_SET_CSR(csr_reg, val)                      \
    ({                                                  \
        long long __tmp = (long long)(val);             \
        _ASM("csrrs %0, " csr_reg ", %1"                \
            : "=r"(__tmp)                               \
            : "rK"(__tmp)                               \
            : "memory");                                \
        __tmp;                                          \
    })


