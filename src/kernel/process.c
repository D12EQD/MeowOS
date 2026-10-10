#include "kernel/process.h"
#include "lib/def.h"
#include "lib/printf.h"

struct process procs[PROCS_MAX]; // 所有进程控制结构

struct process *create_process(uint32_t pc) {
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc){
        printf("no free process slots");
        return NULL;
    }

    // 设置被调用者保存的寄存器。这些寄存器值将在 switch_context 
    // 中的第一次上下文切换时被恢复。
    u32 *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) pc;          // ra

    // 初始化字段
    proc->pid = i;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    return proc;
}
