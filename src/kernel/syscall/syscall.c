#include "kernel/arch/def.h"
#include "kernel/lib/def.h"

#include "kernel/proc/process.h"

#include "kernel/fs/fs.h"

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

u64 sys_fork(){
    return kfork("fork");
}

void syscall(){
    i64 num;
    Process *p = current_process;

    num = p->trap_frame_page->a7;
    if (num >= 0 && num < SYS_COUNT) {
        p->trap_frame_page->a0 = syscall_fn_list[num]();
    } else {
        p->trap_frame_page->a0 = -1;
    }

}

u64 sys_getpid(void){
    return current_process->pid;
}

u64 sys_mmap(void){
    return 0;
}

u64 sys_unmmap(void){
    return 0;
}

u64 sys_exit(void) {
    u64 return_val = get_arg(0);
    kexit(return_val);
    return 0;
}

u64 sys_wait(void) { 
    u64 address = get_arg(0);
    return kwait(address);
}

u64 sys_palloc(void){
    u64 page_count = get_arg(0);
    u64 res = kpalloc(page_count);
    return res;
}

u64 sys_pfree(void){
    u64 page_count = get_arg(0);
    kpfree(page_count); // bool
    return true;
}

u64 sys_write(){
    i64 fd = (i64) get_arg(0);
    void *buf = (void *)get_arg(1);
    u64 len = get_arg(2);
    return kwrite(fd, buf, len);
}

u64 sys_read(void){ 
    i64 fd = (i64) get_arg(0);
    void *buf = (void *)get_arg(1);
    u64 len = get_arg(2);
    return kread(fd, buf, len);
}

u64 sys_open(void){
    const char *name = (const char *)get_arg(0);
    u32 flag = (u32)get_arg(1);
    i64 res = kopen(name, flag);
    return res;
}


u64 sys_close(void){ 
    return 0; 
}

u64 sys_mkdir(void){
    return 0;
}

u64 sys_dup(){
    return 0;
}

u64 sys_dup2(){
    return 0;
}

u64 sys_readdir(){

    return 0;
}

u64 sys_unlink(){
    const char * user_path = (const char *)get_arg(0);
    return kunlink(user_path);
}

u64 sys_pipe(){
    return 0;
}

