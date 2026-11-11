#include "user/def.h"
#include "user/os/syscall.h"

u64 syscall(u64 sysnumber, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    register u64 a0 __asm__("a0") = arg0;
    register u64 a1 __asm__("a1") = arg1;
    register u64 a2 __asm__("a2") = arg2;
    register u64 a3 __asm__("a3") = arg3;
    register u64 a4 __asm__("a4") = arg4;
    register u64 a5 __asm__("a5") = arg5;
    register u64 a7 __asm__("a7") = sysnumber;   // syscall 号放 a7

    __asm__ volatile (
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a7)
        : "memory"
    );

    return a0;
}


u64 getpid(void) {
    return syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0);
}

/* 没有返回值：sys_exit 一旦成功就不回来了 */
void exit(i64 code) {
    syscall(SYS_EXIT, (u64)code, 0, 0, 0, 0, 0);
}

u64 write(i64 fd, void* buf, u64 buf_len){
    return syscall(SYS_WRITE, (u64)fd, (u64)buf, buf_len, 0, 0, 0);
}

u64 read(i64 fd, void* buf, u64 buf_len){
    return syscall(SYS_READ, (u64)fd, (u64)buf, buf_len, 0, 0, 0);
}

u64 fork(){
    return syscall(SYS_FORK, 0, 0, 0, 0, 0, 0);
}

u64 wait(void* ptr){
    return syscall(SYS_WAIT, (u64)ptr, 0, 0, 0, 0, 0);
}

/* 分配连续虚拟内存 */
void* palloc(u64 page_count){
    return (void *)syscall(SYS_PALLOC, page_count, 0, 0, 0, 0, 0);
}

/* 释放连续虚拟内存 */
void pfree(u64 page_count){
    syscall(SYS_PFREE, page_count, 0, 0, 0, 0, 0);
}

i64 open(const char *name, u32 flags){
    return syscall(SYS_OPEN, (u64)name, (u64)flags, 0, 0, 0, 0);
}
