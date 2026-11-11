#pragma once
#include "kernel/lib/printk.h"
#include "kernel/lib/def.h"

/* 自旋锁 */
typedef struct {
    i32 locked;
    char name;
} SpinLock;

// Per-CPU state.
typedef struct {
    i64 interrput_depth;        // 关中断深度
    bool enable_intr;           // 进入中断前的sie
}CpuLock;

extern CpuLock cpu;

#define SPINLOCK_GLOBAL_INIT(lock, cname) SpinLock lock = {.locked = 0, .name = cname}

#define LOCK_IMPLEMENT
#ifndef LOCK_IMPLEMENT

#define spinlock_acquire(l) 0
#define spinlock_release(l) 0
#define critical_enter(l) 0
#define critical_exit(l) 0

#else

void critical_enter();
void critical_exit();

void spinlock_init(SpinLock* lk, char name);
bool spinlock_try_acquire(SpinLock *lk);
void spinlock_release(SpinLock *lk);
void spinlock_acquire(SpinLock *lk);
bool spinlock_display(SpinLock *lk);
#endif
