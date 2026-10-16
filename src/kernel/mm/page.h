#pragma once
// pte : Page Table Entry
// pnn : Physical Page Number
#include "kernel/arch/def.h"
#include "kernel/lib/def.h"

typedef u64 pte_t;

#define PTE_V   (1UL << 0) // valid
#define PTE_R   (1UL << 1) // readable
#define PTE_W   (1UL << 2) // writable
#define PTE_X   (1UL << 3) // executable
#define PTE_U   (1UL << 4) // user ? 
#define PTE_G   (1UL << 5) // 
#define PTE_A   (1UL << 6)
#define PTE_D   (1UL << 7)

#define PTE_PPN_SHIFT   10
#define PTE_PPN_MASK    (0xFFFFFFFFFFFUL << PTE_PPN_SHIFT)  /* 44 位 */

#define PTE_KERNEL  (PTE_R | PTE_W | PTE_X)
#define PTE_USER    (PTE_R | PTE_W | PTE_X | PTE_U)



