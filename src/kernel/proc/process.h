#pragma once
#include "kernel/arch/trap.h"
#include "kernel/lib/def.h"

#define PROCESS_MAX_COUNT 8       // 最大进程数量

enum proc_state{
    PROC_UNUSED,    // 未使用的进程控制结构
    PROC_READY,     // 可运行的进程
    PROC_WAIT,      // 阻塞态度
    PROC_RUNNING,   // 运行态
};

struct context {
    u64 ra;
    u64 sp;
    u64 s0;
    u64 s1;
    u64 s2;
    u64 s3;
    u64 s4;
    u64 s5;
    u64 s6;
    u64 s7;
    u64 s8;
    u64 s9;
    u64 s10;
    u64 s11;
};

struct process {
    int pid;                    // 进程 ID
    enum proc_state state;      // 进程状态
    struct trap_frame frame;    // 陷入的保存栈帧  TODO : use `kmalloc` to dynamic alloc this frame
    vaddr_t kernel_sp;          // 栈指针
};
