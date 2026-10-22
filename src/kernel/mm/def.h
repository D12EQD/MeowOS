#pragma once
#include "kernel/arch/def.h"
/* 对虚拟内存和物理内存如何布局进行定义 */

/* 物理内存定义 */
#define MAX_PADDR (__free_ram_end)
#define MIN_PADDR (0x80200000UL)

#define kERNEL_TEXT_MIN_PADDR MIN_PADDR
#define KERNLE_TEXT_MAX_PADDR (__end_text_section)

// UART 硬件信息
#define UART0     0x10000000L
#define UART0_IRQ 10


/* 虚拟内存定义 */
#define MAX_VADDR (1UL << 38)   // 最大虚拟地址
#define PAGE_SIZE 4096
#define PAGE_SIZE_LOG2 12

#define TRAMPOLINE_VADDR (MAX_VADDR - PAGE_SIZE)        // trampoline 帧最高
#define TRAPFRAME_VADDR (TRAMPOLINE_VADDR - PAGE_SIZE)  // trap 帧第二高
#define KERNEL_STACK(p) (TRAPFRAME_VADDR - ((p) + 1) * 2 * PAGE_SIZE)

/* function */
#define MAKE_SATP(page) ((8L << 60) | (((u64)page) >> 12))





