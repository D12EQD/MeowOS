#include "kernel/arch/def.h"

#include "kernel/lib/def.h"

#include "kernel/proc/process.h"

#include "kernel/syscall/syscall.h"

#include "kernel/mm/page.h"


static u64 get_arg(int n) {
    Process *p = current_process;
    switch (n) {
        case 0:
            return p->trap_frame_page->a0;
        case 1:
            return p->trap_frame_page->a1;
        case 2:
            return p->trap_frame_page->a2;
        case 3:
            return p->trap_frame_page->a3;
        case 4:
            return p->trap_frame_page->a4;
        case 5:
            return p->trap_frame_page->a5;
    }
    PANIC("get_arg");
    return -1;
}

/*
 * ssize_t write(const void buf[count], size_t count);
 */
u64 sys_write(){
    char *buf = (char *)vaddr_to_paddr(current_process->page_table, get_arg(1));
    u64 count = get_arg(2);

    // TEST :
    puts_n(buf, count);
    return 1;
}

void syscall(){
    int num;
    Process *p = current_process;

    num = p->trap_frame_page->a7;
    if (num > 0 && num < SYS_COUNT && syscall_name[num]) {
        // Use num to lookup the system call function for num, call it,
        // and store its return value in p->trap_frame_page->a0
        syscall_fn_t fn = (syscall_fn_t)syscall_name[num];
        p->trap_frame_page->a0 = fn();
    } else {
        p->trap_frame_page->a0 = -1;
    }
}
