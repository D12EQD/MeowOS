#pragma once

// pte : Page Table Entry 页表项
// pnn : Physical Page Number 页框编号
// pt  : Page Table 

/*
 
页目录 (Page Directory) / 页表 (Page Table)
索引 (Index)   页表项 (Page Table Entry - PTE)
+---------+---------------------------------------------+
|    0    |  [ PPN (物理页号) | Flags (权限标志) ]      |
+---------+---------------------------------------------+
|    1    |  [ PPN (物理页号) | Flags (权限标志) ]      |
+---------+---------------------------------------------+
|   ...   |                   ...                       |
+---------+---------------------------------------------+
|   511   |  [ PPN (物理页号) | Flags (权限标志) ]      |
+---------+---------------------------------------------+


63       53 52                  10 9     8  7   6   5   4   3   2   1   0
+----------+----------------------+-------+---+---+---+---+---+---+---+---+
| Reserved |     PPN (物理页号)   | RSWD  | D | A | G | U | X | W | R | V |
+----------+----------------------+-------+---+---+---+---+---+---+---+---+
  11 bit           44 bit          2 bit  \_____________ 10 bit ________/


Sv39 的虚拟地址
63      39 38      30 29      21 20      12 11       0
+---------+----------+----------+----------+----------+
| 0...0   |  VPN[2]  |  VPN[1]  |  VPN[0]  |  Offset  |
+---------+----------+----------+----------+----------+
            (9 bits)   (9 bits)   (9 bits)   (12 bits)
*/

#include "kernel/arch/def.h"
#include "kernel/lib/def.h"
#include "kernel/mm/def.h"

#define __PAGE_SIZE 4096
#define __PAGE_LOG_SIZE 12

#define PTE_V   (1UL << 0) // valid
#define PTE_R   (1UL << 1) // readable
#define PTE_W   (1UL << 2) // writable
#define PTE_X   (1UL << 3) // executable
#define PTE_U   (1UL << 4) // user ? 
#define PTE_G   (1UL << 5) // global
#define PTE_A   (1UL << 6) // accessed
#define PTE_D   (1UL << 7) // dirty

#define PAGE_VADD_LV 9

#define GET_VA_OFFSET(va)       __get_bit(va, 0, 11)
#define GET_VA_VPN0(va)         __get_bit(va, 12, 20)
#define GET_VA_VPN1(va)         __get_bit(va, 21, 29)
#define GET_VA_VPN2(va)         __get_bit(va, 30, 38)
#define GET_VA_VPN(va, level)   __get_bit(va, 12 + PAGE_VADD_LV * level, 12 + PAGE_VADD_LV * level + 8)

#define GET_VA_RESERVE(va)      __get_bit(va, 39, 63)

#define PADDR_TO_PTE(pa)         ( (pte_t)((((u64)pa) >> 12) << 10) )
#define PTE_TO_PADDR(pte)        ( (pt_t)(((pte) >> 10) << 12) )

typedef u64 pte_t;     // 页表项
typedef pte_t* pt_t;   // 页表

void kernel_page_init();
bool mappages(pt_t page_table, u64 va, u64 page_count, u64 paddr, u64 permit_flag);

pte_t* walk(pt_t pt_addr, bool is_alloc, u64 vaddr);
void free_walk(pt_t page_table, int depth);

void* kernel_alloc_page_table();
void* user_alloc_page_table();

void user_unmap_pages(pt_t page_table, u64 start, u64 page_count, bool is_free);


void kernel_page_open();
u64 vaddr_to_paddr(pt_t page_table, u64 vaddr);

extern pt_t kernel_page_table; /* 内核页目录 */

