#pragma once
#include "kernel/lib/def.h"

/*
 * 系统调用清单 —— 唯一真相源
 * 新增一个系统调用，只需在这里加一行 X(...)，其余全部自动生成
 */

#pragma once
#include "kernel/lib/def.h"

// ========================== X define ==========================
#define SYSCALL_LIST(X)          \
    X(READ,    read,   0)        \
    X(WRITE,   write,  1)        \
    X(OPEN,    open,   2)        \
    X(CLOSE,   close,  3)        \
    X(FSTAT,   fstat,  4)        \
    X(FORK,    fork,   5)        \
    X(EXEC,    exec,   6)        \
    X(WAIT,    wait,   7)        \
    X(EXIT,    exit,   8)        \
    X(GETPID,  getpid, 9)        \
    X(MMAP,    mmap,   10)       \
    X(UNMMAP,  unmmap, 11)

typedef u64 (*syscall_fn_t)(void);

/* 编号 */
#define __SC_ENUM(upper, lower, num) SYS_##upper = num,
enum SyscallNumber {
    SYSCALL_LIST(__SC_ENUM)
    SYS_COUNT
};
#undef __SC_ENUM

/* 内核侧函数原型 */
#define __SC_DECL(upper, lower, num) u64 sys_##lower(void);
SYSCALL_LIST(__SC_DECL)
#undef __SC_DECL

/* 名字表（调试打印用） */
#define __SC_NAME(upper, lower, num) [SYS_##upper] = #lower,
static const char *const syscall_name[] = {
    SYSCALL_LIST(__SC_NAME)
};
#undef __SC_NAME

/* 函数表 —— 真正被 syscall() 调用的那张 */
#define __SC_FN(upper, lower, num) [SYS_##upper] = sys_##lower,
static const syscall_fn_t syscall_fn_list[] = {
    SYSCALL_LIST(__SC_FN)
};
#undef __SC_FN

typedef u64 (*syscall_fn_t)(void);

void syscall();

