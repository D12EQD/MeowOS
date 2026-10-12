#pragma once
#include "lib/def.h"

#define PROCESS_MAX_COUNT 8       // 最大进程数量

enum proc_state{
    PROC_UNUSED,     // 未使用的进程控制结构
    PROC_RUNNABLE,   // 可运行的进程
};

struct process {
    int pid;                // 进程 ID
    enum proc_state state;  // 进程状态: PROC_UNUSED 或 PROC_RUNNABLE
    vaddr_t sp;             // 栈指针
    u8 stack[8192];         // 内核栈
};

void switch_context(u64 *prev_sp, u64 *next_sp);
struct process *create_process(u64 pc);
void yield(void);
void process_init();
