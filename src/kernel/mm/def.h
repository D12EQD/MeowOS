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
#define MAX_VADDR (1UL << 38) // 最大虚拟地址
#define PAGE_SIZE 4096
#define PAGE_SIZE_LOG2 12

#define TRAMPOLINE_VADDR    (MAX_VADDR - PAGE_SIZE)        // trampoline 帧最高
#define TRAPFRAME_VADDR     (TRAMPOLINE_VADDR - PAGE_SIZE)  // trap 帧第二高
#define KERNEL_STACK(p)     (TRAPFRAME_VADDR - ((p) + 1) * 2 * PAGE_SIZE)

/* 
 * 用户虚拟内存定义
 * 0       陷阱页面
 * 1 - 3   全局读写
 * 4 - x   代码区域
 * x+1     陷阱页面
 * x+2 - ? 堆区域，由 kpalloc / kpfree 管理
 *
 * x 由用户镜像大小决定，见下面的 USER_CODE_PAGE_COUNT。
 */

// user image
#ifndef __ASSEMBLER__

EXTERN_SYMBOL(_binary_user_bin_start);
EXTERN_SYMBOL(_binary_user_bin_end);

#endif

#define USER_IMAGE_SIZE ((u64)_binary_user_bin_end - (u64)_binary_user_bin_start)

// user vitual address
#define UESR_VADDR_START (PAGE_SIZE)
#define USER_CODE_START (PAGE_SIZE * 4)

#define UESR_DATA_PAGE_START UESR_VADDR_START
#define USER_DATA_PAGE_COUNT ((USER_CODE_START - UESR_VADDR_START) / PAGE_SIZE)
#define USER_CODE_PAGE_COUNT ((USER_IMAGE_SIZE - USER_DATA_PAGE_COUNT * PAGE_SIZE + PAGE_SIZE - 1) / PAGE_SIZE)
#define USER_GUARD_VADDR (USER_CODE_START + USER_CODE_PAGE_COUNT * PAGE_SIZE)
#define UESR_HEAP_START (USER_GUARD_VADDR + PAGE_SIZE)

/* 用户镜像魔数 用来确认 user.bin 确实从 UESR_VADDR_START 开始 */
#define USER_IMAGE_MAGIC 0x4d454f57

/* function */
#define MAKE_SATP(page) ((8L << 60) | (((u64)page) >> 12))
