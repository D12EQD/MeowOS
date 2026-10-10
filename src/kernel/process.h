#pragma once
#include "lib/def.h"

#define PROCS_MAX 8       // 最大进程数量
#define PROC_UNUSED   0   // 未使用的进程控制结构
#define PROC_RUNNABLE 1   // 可运行的进程

struct process {
    int pid;             // 进程 ID
    int state;           // 进程状态: PROC_UNUSED 或 PROC_RUNNABLE
    vaddr_t sp;          // 栈指针
    u8 stack[8192]; // 内核栈
};
