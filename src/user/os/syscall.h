#pragma once
#include "user/def.h"
/*
 * 系统调用清单 —— 唯一真相源
 * 格式：X(大写名, 小写名, 编号)
 * 编号显式指定，保证 ABI 稳定；新增只能往后追加，不能插在中间。
 */


// ========================== X define ==========================
#define SYSCALL_LIST(X)             \
    X(READ,     read,   0)          \
    X(WRITE,    write,  1)          \
    X(OPEN,     open,   2)          \
    X(CLOSE,    close,  3)          \
    X(FORK,     fork,   5)          \
    X(WAIT,     wait,   6)          \
    X(EXIT,     exit,   7)          \
    X(GETPID,   getpid, 8)          \
    X(PALLOC,   palloc, 9)          \
    X(PFREE,    pfree, 10)

/* 编号 */
#define __SC_ENUM(upper, lower, num) SYS_##upper = num,
enum SyscallNumber {
    SYSCALL_LIST(__SC_ENUM)
    SYS_COUNT
};
#undef __SC_ENUM

u64 fork();
u64 read(i64 fd, void* buf, u64 buf_len);
u64 write(i64 fd, void* buf, u64 buf_len);
u64 wait(void* ptr);

u64 syscall(u64 sysnumber, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);


