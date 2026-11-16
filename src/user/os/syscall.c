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

i64 mkdir(const char *name){
    return syscall(SYS_MKDIR, (u64)name, 0, 0, 0, 0, 0);
}

i64 unlink(const char *name){
    return syscall(SYS_UNLINK, (u64)name, 0, 0, 0, 0, 0);
}

i64 dup(i64 fd){
    return syscall(SYS_DUP, (u64)fd, 0, 0, 0, 0, 0);
}

i64 dup2(i64 oldfd, i64 newfd){
    return syscall(SYS_DUP2, (u64)oldfd, (u64)newfd, 0, 0, 0, 0);
}

i64 readdir(i64 fd, DirEntry *out){
    return syscall(SYS_READDIR, (u64)fd, (u64)out, 0, 0, 0, 0);
}

i64 close(i64 fd){
    return syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
}

i64 lseek(i64 fd, i64 offset, i64 flag){
    return syscall(SYS_LSEEK, fd, offset, flag, 0, 0, 0);
}

/* fd1 读管道 fd2 写管道 */
i64 pipe(i64* fd1, i64* fd2){
    return syscall(SYS_PIPE, (u64)fd1, (u64)fd2, 0, 0, 0, 0);
}

/* 内核诊断快照，只读 */
i64 diag(Kstat *out){
    return (i64)syscall(SYS_DIAG, (u64)out, sizeof(Kstat), 0, 0, 0, 0);
}
