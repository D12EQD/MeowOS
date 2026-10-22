#include "kernel/start.h"

#include "kernel/lib/printk.h"
#include "kernel/lib/string.h"
#include "kernel/lib/def.h"

#include "kernel/arch/timer.h"
#include "kernel/arch/trap.h"

#include "kernel/device/fdt.h"
#include "kernel/arch/def.h"

#include "kernel/mm/page.h"
#include "kernel/mm/pmm.h"

EXTERN_SYMBOL( __end_text_section);
EXTERN_SYMBOL( __trampoline_section_start);
EXTERN_SYMBOL( __trampoline_section_end);
EXTERN_SYMBOL( __bss);
EXTERN_SYMBOL( __bss_end);
EXTERN_SYMBOL( __stack_bottom);
EXTERN_SYMBOL( __stack_top);
EXTERN_SYMBOL( __free_ram);
EXTERN_SYMBOL( __free_ram_end);

BootInfo boot_info;

void kernel_init(u64 fdt_address){
    WRITE_CSR(CSR_SATP, 0); // 关闭分页
    bss_init();
    trap_init();
    kernel_fdt_init((void *)fdt_address, &boot_info);
    kmm_init();
    kernel_page_init();
    kernel_page_open();

    // timer_init(&boot_info);
}

void kernel_start(){
    u64 hartid, fdt_address;
    _ASM(
        "mv %0, s0\n\t"
        "mv %1, s1\n\t"
        : "=r"(hartid), "=r"(fdt_address)
    );
    kernel_init(fdt_address);
    
    u64 start_mm = 0x80200000;
    u64 end_mm = (u64)__free_ram_end;

    for (u64 i = start_mm; i < end_mm; i += PAGE_SIZE / 2){
        if (i != vaddr_to_paddr(kernel_page_table, i)){
            prln_debug("address error %lx", i); while(1);
        }
    }
    pr_debug("hello world\n");

    while (1);
}

