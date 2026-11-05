#include "kernel/arch/def.h"
#include "kernel/lib/def.h"

#include "kernel/proc/process.h"

#include "kernel/syscall/syscall.h"

#include "kernel/mm/page.h"

// TODO : 切换为文件形式的时候需要分配内核缓冲区 目前暂时硬编码测试是否可以正常读写操作
#define WRITE_CHUNK_SIZE 256
char buf[WRITE_CHUNK_SIZE];


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

u64 sys_write(){
    i64 fd = get_arg(0);
    if (fd != 1) return -1; // 目前只支持 stdout，非法 fd 返回 -1 而不是 PANIC

    u64 user_buf = get_arg(1);
    u64 count = get_arg(2);
    u64 done = 0;

    // 一批一批写
    while (done < count){
        u64 n = count - done;
        if (n > sizeof(buf)) n = sizeof (buf);

        if (copy_from_user(current_process->page_table, buf, (void *)(user_buf + done), n) != (i64)n){
            return done ? done : (u64)-1;
        }

        puts_n(buf, n);
        done += n;
    }

    return count;
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
    return kpalloc(page_count);
}

u64 sys_pfree(void){
    u64 page_count = get_arg(0);
    return kpfree(page_count); // bool
}

u64 sys_read(void)   { return 0; }
u64 sys_open(void)   { return 0; }
u64 sys_close(void)  { return 0; }
