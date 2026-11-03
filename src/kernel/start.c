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
    interrupt_on();
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

void test_page(Process *proc){
    for (u64 va = TRAMPOLINE_VADDR , pa = (u64)trampoline, i = 0; i < PAGE_SIZE; i ++, va ++, pa ++){
        ASSERT(vaddr_to_paddr(kernel_page_table, va) == pa);
    }
    
    for (u64 va = TRAMPOLINE_VADDR , pa = (u64)trampoline, i = 0; i < PAGE_SIZE; i ++, va ++, pa ++){
        ASSERT(vaddr_to_paddr(proc->page_table, va) == pa);
    }
    
    for (u64 va = TRAPFRAME_VADDR , pa = (u64)(proc->trap_frame_page), i = 0; i < PAGE_SIZE; i ++, va ++, pa ++){
        ASSERT(vaddr_to_paddr(proc->page_table, va) == pa);
    }

    pr_debug(
        "trampoline_vaddr is %lx\ntrapframe_vaddr is %lx\n",
        TRAMPOLINE_VADDR, TRAPFRAME_VADDR
    );

    pr_debug("kernel_stack_page_start is %lx\n", KERNEL_STACK(0));
    pr_debug("kernel_stack_page_end is %lx\n", KERNEL_STACK(PROCESS_MAX_COUNT));

    pr_debug("process_stack_page_start is %lx\n", PROCESS_STACK_PAGE_START);
    pr_debug("process_stack_page_end is %lx\n", PROCESS_STACK_PAGE_END);
}

void test_copy_page(){
    Process* fa = create_user_process();
    first_user_init(fa);

    current_process = fa;
    Process* son = get_proc_from_pid(kfork("son"));
    
    pr_debug("fa alloc page %lu", fa->alloc_page_count);

    for (u64 i = 0; i < son->alloc_page_count; i ++){
        u64 va = USER_VADDR_START + i * PAGE_SIZE;
        u64 son_pa = vaddr_to_paddr(son->page_table, va);
        u64 fa_pa = vaddr_to_paddr(fa->page_table, va);

        // pr_debug("va = %lx, son_pa = %lx fa_pa = %lx", va, son_pa, fa_pa);

        ASSERT(memcmp((void *)son_pa, (void *)fa_pa, PAGE_SIZE) == 0);
    }
    ASSERT(fa->alloc_page_count == son->alloc_page_count);

    current_process = get_proc_from_pid(0); // 空闲进程
    // pr_debug("passed test_copy_page\n");
}

void kernel_main(){
    // set_print_filter(K_DEBUG);
    // test_copy_page();
    Process* p = create_user_process();
    first_user_init(p);
    scheduler();
}
