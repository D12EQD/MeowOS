#include "kernel/arch/trap.h"
#include "kernel/arch/def.h"

#include "kernel/proc/lock.h"

CpuLock cpu; // 管理该cpu互斥资源

/* 进入关中断临界区 */
void critical_enter(){
    u64 old = interrupt_off() & SSTATUS_SIE;

    if (cpu.interrput_depth == 0){
        // 首次进入临界区
        cpu.enable_intr = (old != 0);
        // status寄存器的sie位置为 1 说明可以开启中断
    }
    cpu.interrput_depth ++;
}

/* 退出关中断临界区 */
void critical_exit(){
    u64 sie = 0;
    // 发现进入关中断临界区没有进入直接退出
    ASSERT(is_enable_interrupt(&sie));
    
    cpu.interrput_depth --;
    if (cpu.interrput_depth < 0){ // error
        PANIC("critical_exit error : interrput_depth");
    }
    
    // 如果可以所有进程或线程退出临界区并且可以开启中断
    // 就开启中断
    if (cpu.interrput_depth == 0 && cpu.enable_intr){
        cpu.enable_intr = 0;
        interrupt_on();
    }
}

void spinlock_init(SpinLock* lk, const char *name){
    critical_enter();
    lk->locked = 0; 
    lk->name = name;
    critical_exit();
}

/* 获取自旋锁 */
void spinlock_acquire(SpinLock *lk) {
    critical_enter();
    while (__atomic_load_n(&lk->locked, __ATOMIC_RELAXED) != 0){
        critical_exit();
        critical_enter();
    }
    while (__atomic_exchange_n(&lk->locked, 1, __ATOMIC_ACQUIRE) != 0){
        critical_exit();
        critical_enter();
    }
}

/* 
 * 尝试获取自旋锁的值，如果发现可以获取到锁则一直等待 (乐观锁的思想)
 * 如果结果为false 不需要调用 spinlock_release 释放锁，失败时不会获取锁
 */
bool spinlock_try_acquire(SpinLock *lk) {
    critical_enter();

    bool ok = __atomic_load_n(&lk->locked,  __ATOMIC_ACQUIRE);

    if (ok){
        critical_exit();
        return false;
    }

    while (__atomic_exchange_n(&lk->locked, 1, __ATOMIC_ACQUIRE) != 0){
        critical_exit();
        critical_enter();
    }

    return true;
}

/* 释放自旋锁 */
void spinlock_release(SpinLock *lk) {
    ASSERT(lk->locked != 0);
    __atomic_store_n(&lk->locked, 0, __ATOMIC_RELEASE);
    critical_exit();
}

void spinlock_debug(SpinLock *lk){
    if (lk->name) pr_cont_debug("%s : ", lk->name);
    pr_debug("The locked of lock is %u", lk->locked);
}
