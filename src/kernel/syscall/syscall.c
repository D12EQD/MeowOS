#include "kernel/arch/def.h"
#include "kernel/lib/def.h"
#include "kernel/proc/process.h"

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
    
}

void syscall(){
    Process * p = current_process;

}
