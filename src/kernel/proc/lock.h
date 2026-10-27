#pragma once
#include "kernel/lib/def.h"

/* 自旋锁 */
typedef struct {
    i32 locked;
    const char *name;
} SpinLock;

// Per-CPU state.
typedef struct {
    u64 interrput_depth;        // 关中断深度
    bool enable_intr;           // 老sie
}CpuLock;


#define SPINLOCK_GLOBAL_INIT(lock) SpinLock lock = {.locked = 0}

void critical_enter();
void critical_exit();

void spinlock_init(SpinLock* lk, const char *name);
void spinlock_acquire(SpinLock *lk);
bool spinlock_try_acquire(SpinLock *lk);
void spinlock_release(SpinLock *lk);
void spinlock_debug(SpinLock *lk);
