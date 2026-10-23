#pragma once
#include "kernel/lib/def.h"
#include "kernel/proc/process.h"

/* 自旋锁 */
typedef struct {
    u32 lock;
    Cpu *cpu;
    char *name;      // debug
} SpinLock;

void critical_enter();
void critical_exit();
void get_spin_lock();
void 
