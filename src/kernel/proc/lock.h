#pragma once
#include "kernel/lib/def.h"

/* 自旋锁 */
typedef struct {
    i32 locked;
} SpinLock;

#define SPINLOCK_GLOBAL_INIT(lock) SpinLock lock = {.locked = 0}

void critical_enter();
void critical_exit();

void spinlock_init(SpinLock* lk);
void spinlock_acquire(SpinLock *lk);
bool spinlock_try_acquire(SpinLock *lk);
void spinlock_release(SpinLock *lk);
