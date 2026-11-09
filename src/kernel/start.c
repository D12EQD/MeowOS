#include "kernel/start.h"
#include "kernel/arch/def.h"

#include "kernel/lib/printk.h"
#include "kernel/lib/string.h"
#include "kernel/lib/def.h"

#include "kernel/arch/timer.h"
#include "kernel/arch/trap.h"

#include "kernel/device/fdt.h"

#include "kernel/proc/process.h"

#include "kernel/mm/page.h"
#include "kernel/mm/pmm.h"

#include "kernel/fs/file.h"

EXTERN_SYMBOL( __end_text_section);
EXTERN_SYMBOL( __trampoline_section_start);
EXTERN_SYMBOL( __trampoline_section_end);
EXTERN_SYMBOL( __bss);
EXTERN_SYMBOL( __bss_end);
EXTERN_SYMBOL( __stack_bottom);
EXTERN_SYMBOL( __stack_top);
EXTERN_SYMBOL( __free_ram);
EXTERN_SYMBOL( __free_ram_end);

EXTERN_SYMBOL(trampoline);

BootInfo boot_info;

void kernel_main();
void kernel_start();
void kernel_init(u64 fdt_address);

void kernel_init(u64 fdt_address){
    WRITE_CSR(CSR_SATP, 0); // 关闭分页
    bss_init();
    trap_init();
    kernel_fdt_init((void *)fdt_address, &boot_info);
    kmm_init();
    kernel_page_init();
    kernel_page_open();

    process_init();
    timer_init(&boot_info);
}

void kernel_start(){
    u64 hartid, fdt_address;
    _ASM(
        "mv %0, s0\n\t"
        "mv %1, s1\n\t"
        : "=r"(hartid), "=r"(fdt_address)
    );
    WRITE_REG(tp, hartid);
    kernel_init(fdt_address);
    kernel_main();
}

void kernel_main(){
    // set_print_filter(K_DEBUG);
    // test_copy_page();
    Process* p = create_user_process();

    // PANIC("DirEntry size = %ld", sizeof(DirEntry));

    first_user_init(p);
    scheduler();
}
