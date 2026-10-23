#pragma once
#include "kernel/arch/trap.h"
#include "kernel/mm/page.h"
#include "kernel/lib/def.h"

#define PROCESS_MAX_COUNT 64       // 最大进程数量

enum proc_state{
    PROC_UNUSED,    // 未使用的进程控制结构
    PROC_READY,     // 可运行的进程
    PROC_WAIT,      // 阻塞态度
    PROC_RUNNING,   // 运行态
    PROC_ZOMBIE,    // 僵尸形态
};

typedef struct {
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
}ThreadCtx;

typedef struct __attribute__((packed)) {
  /*   0 */ u64 kernel_satp;        // kernel page table
  /*   8 */ u64 kernel_sp;          // top of process's kernel stack
  /*  16 */ u64 user_trap_function; // user_trap_handler()
  /*  24 */ u64 epc;                // saved user program counter
  /*  32 */ u64 kernel_hartid;      // saved kernel tp
  /*  40 */ u64 ra;
  /*  48 */ u64 sp;
  /*  56 */ u64 gp;
  /*  64 */ u64 tp;
  /*  72 */ u64 t0;
  /*  80 */ u64 t1;
  /*  88 */ u64 t2;
  /*  96 */ u64 s0;
  /* 104 */ u64 s1;
  /* 112 */ u64 a0;
  /* 120 */ u64 a1;
  /* 128 */ u64 a2;
  /* 136 */ u64 a3;
  /* 144 */ u64 a4;
  /* 152 */ u64 a5;
  /* 160 */ u64 a6;
  /* 168 */ u64 a7;
  /* 176 */ u64 s2;
  /* 184 */ u64 s3;
  /* 192 */ u64 s4;
  /* 200 */ u64 s5;
  /* 208 */ u64 s6;
  /* 216 */ u64 s7;
  /* 224 */ u64 s8;
  /* 232 */ u64 s9;
  /* 240 */ u64 s10;
  /* 248 */ u64 s11;
  /* 256 */ u64 t3;
  /* 264 */ u64 t4;
  /* 272 */ u64 t5;
  /* 280 */ u64 t6;
} TrapFrame ;

typedef struct{
    int pid;                    // 进程 ID
    pt_t page_table;            // page table 页表
    enum proc_state state;      // 进程状态
    TrapFrame* trap_frame_page; // 陷入的保存栈帧
    u64 kstack;                 // 栈指针
    u64 alloc_page_count;       // 已经分配的页面数量
    ThreadCtx context;          // 线程上下文
}Process;

// Per-CPU state.
typedef struct {
    Process *proc;              // 同上
    ThreadCtx context;          // 同上
    u64 interrput_depth;        // 关中断深度
    u64 old_sie;                // 老sie
}Cpu;

void yield();
void schedule();
Process *create_user_process();
Cpu* get_cpu();
