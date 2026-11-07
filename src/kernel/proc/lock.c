#include "kernel/arch/trap.h"
#include "kernel/arch/def.h"

#include "kernel/proc/lock.h"
#include "kernel/proc/process.h"

CpuLock cpu; // 管理该cpu互斥资源

#ifdef LOCK_IMPLEMENT


/* 可重入 进入关中断临界区 */
void critical_enter(){
    u64 old = interrupt_off() & SSTATUS_SIE;

    if (cpu.interrput_depth == 0){
        // 首次进入临界区 记录是否可以关中断
        // status寄存器的sie位置为 1 说明可以开启中断
        cpu.enable_intr = (old != 0);
    }
    cpu.interrput_depth ++;
}

/* 可重入 退出关中断临界区 */
void critical_exit(){
    u64 sie = 0;
    // 发现进入关中断临界区没有进入直接退出
    ASSERT(is_enable_interrupt(&sie));
    
    cpu.interrput_depth --;
    if (cpu.interrput_depth < 0){ // error
        PANIC("critical_exit error : interrput_depth");
    }
    
    if (cpu.interrput_depth == 0 && cpu.enable_intr){
        cpu.enable_intr = 0;
        interrupt_on();
    }
}

void spinlock_init(SpinLock* lk, char name){
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
 * returns : 
 *      false 不需要调用 spinlock_release 释放锁，失败时不会获取锁
 *      true 获取到锁
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

bool spinlock_debug(SpinLock *lk){
    pr_cont_debug("Lock : %c", lk->name);
    pr_debug(" locknumber = %u", lk->locked);
    return false;
}


#endif
