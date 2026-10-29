#pragma once
#include "kernel/lib/def.h"

/*
 * 系统调用清单 —— 唯一真相源
 * 新增一个系统调用，只需在这里加一行 X(...)，其余全部自动生成
 */
#define SYSCALL_LIST(X) \
    X(READ,  read)      \
    X(WRITE, write)     \
    X(OPEN,  open)      \
    X(CLOSE, close)     

/* 系统调用号 */
#define __SYSCALL_ENUM(upper, lower) SYS_##upper,
enum SystemCall {
    SYSCALL_LIST(__SYSCALL_ENUM)
    SYS_COUNT
};
#undef __SYSCALL_ENUM

/* 内核侧函数原型 */
#define __SYSCALL_DECL(upper, lower) u64 sys_##lower(void);
SYSCALL_LIST(__SYSCALL_DECL)
#undef __SYSCALL_DECL


/* 名字表 */
#define __SYSCALL_NAME(upper, lower) [SYS_##upper] = #lower,
static const char *const syscall_name[] = {
    SYSCALL_LIST(__SYSCALL_NAME)
};
#undef __SYSCALL_NAME

typedef u64 (*syscall_fn_t)(void);

void syscall();

