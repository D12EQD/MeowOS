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
    u64 interrput_depth;        // 关中断深度
    bool enable_intr;           // 进入中断前的sie
}CpuLock;

extern CpuLock cpu;

#define SPINLOCK_GLOBAL_INIT(lock) SpinLock lock = {.locked = 0}

#define LOCK_IMPLEMENT
#ifdef LOCK_IMPLEMENT

void critical_enter();
void critical_exit();

void spinlock_init(SpinLock* lk, char name);
void spinlock_acquire(SpinLock *lk);
bool spinlock_try_acquire(SpinLock *lk);
void spinlock_release(SpinLock *lk);
bool spinlock_debug(SpinLock *lk);

#else

#define critical_enter() 1
#define critical_exit() 1
 
#define spinlock_init( lk, name) 1
#define spinlock_acquire(lk) 1
#define spinlock_try_acquire(lk) 1
#define spinlock_release(lk) 1
#define spinlock_debug(lk) 1

#endif
