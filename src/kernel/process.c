#include "kernel/process.h"
#include "mm/pmm.h"
#include "lib/def.h"
#include "lib/printf.h"

struct process process_list[PROCESS_MAX_COUNT]; // 所有进程控制结构
struct process *current_proc; // 当前运行的进程

void process_init(){
    current_proc = process_list;
}

struct process *create_process(u64 pc) {
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (process_list[i].state == PROC_UNUSED) {
            proc = &process_list[i];
            break;
        }
    }

    if (!proc){
        printf("no free process slots");
        return NULL;
    }

    // 设置被调用者保存的寄存器。这些寄存器值将在 switch_context 中的第一次上下文切换时被恢复。
    u64 *sp = (u64 *) &proc->stack[sizeof(proc->stack)];
    sp -= 16;                 // 分配 16 个 u64 槽（128 字节）
    sp[0]  = (u64) pc;        // ra  ← 进程入口
    sp[1]  = 0;               // s0
    sp[2]  = 0;               // s1
    sp[3]  = 0;               // s2
    sp[4]  = 0;               // s3
    sp[5]  = 0;               // s4
    sp[6]  = 0;               // s5
    sp[7]  = 0;               // s6
    sp[8]  = 0;               // s7
    sp[9]  = 0;               // s8
    sp[10] = 0;               // s9
    sp[11] = 0;               // s10
    sp[12] = 0;               // s11
    sp[13] = 0;               // padding
    sp[14] = 0;               // padding
    sp[15] = 0;               // padding

    // 初始化字段
    proc->pid = i;
    proc->state = PROC_RUNNABLE;
    proc->sp = (u64) sp;
    return proc;
}

__attribute__((naked)) 
void switch_context(u64 *prev_sp, u64 *next_sp) {
     _ASM (
        // 将被调用者保存寄存器保存到当前进程的栈上
        "addi sp, sp, -16 * 8\n"   // 为13个8字节寄存器分配栈空间 多空出3个8字节
        "sd ra,  0  * 8(sp)\n"  // 仅保存被调用者保存的寄存器
        "sd s0,  1  * 8(sp)\n"
        "sd s1,  2  * 8(sp)\n"
        "sd s2,  3  * 8(sp)\n"
        "sd s3,  4  * 8(sp)\n"
        "sd s4,  5  * 8(sp)\n"
        "sd s5,  6  * 8(sp)\n"
        "sd s6,  7  * 8(sp)\n"
        "sd s7,  8  * 8(sp)\n"
        "sd s8,  9  * 8(sp)\n"
        "sd s9,  10 * 8(sp)\n"
        "sd s10, 11 * 8(sp)\n"
        "sd s11, 12 * 8(sp)\n"

        // 切换栈指针
        "sd sp, (a0)\n"         // *prev_sp = sp;
        "ld sp, (a1)\n"         // 在这里切换栈指针(sp)

        // 从下一个进程的栈中恢复被调用者保存的寄存器
        "ld ra,  0  * 8(sp)\n"  // 仅恢复被调用者保存的寄存器
        "ld s0,  1  * 8(sp)\n"
        "ld s1,  2  * 8(sp)\n"
        "ld s2,  3  * 8(sp)\n"
        "ld s3,  4  * 8(sp)\n"
        "ld s4,  5  * 8(sp)\n"
        "ld s5,  6  * 8(sp)\n"
        "ld s6,  7  * 8(sp)\n"
        "ld s7,  8  * 8(sp)\n"
        "ld s8,  9  * 8(sp)\n"
        "ld s9,  10 * 8(sp)\n"
        "ld s10, 11 * 8(sp)\n"
        "ld s11, 12 * 8(sp)\n"
        "addi sp, sp, 16 * 8\n"  // 我们已从栈中弹出13个8字节寄存器
        "ret\n"
    );
}

/*
 * 进程让出 cpu
 * 调用上下文切换函数实现上下文切换
 */
void yield(void) {
    // 搜索可运行的进程
    struct process *next = current_proc;
    
    for (int i = 1; i <= PROCESS_MAX_COUNT; i++) {
        struct process *proc = &process_list[(current_proc->pid + i) % PROCESS_MAX_COUNT];
        if (proc->state == PROC_RUNNABLE) {
            next = proc;
            break;
        }
    }

    // 如果除了当前进程外没有可运行的进程，返回并继续处理
    printf("The proc state is %d , ptr is %p\n", next->state, next);
    if (next == current_proc){
        return;
    }

    // 上下文切换
    struct process *prev = current_proc;
    current_proc = next;
   
    printf("switch ctx start\n");
    switch_context(&prev->sp, &next->sp);
    printf("switch ctx ending\n");

    return;
}
