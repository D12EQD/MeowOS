#include "kernel/arch/trap.h"
#include "kernel/arch/def.h"

#include "kernel/proc/lock.h"
#include "kernel/proc/process.h" 

Cpu cpu;

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

void spinlock_init(SpinLock* lk){
    critical_enter();
    lk->locked = 0; 
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

/* 尝试获取自旋锁 */
bool spinlock_try_acquire(SpinLock *lk) {
    critical_enter();
    if (__atomic_exchange_n(&lk->locked, 1, __ATOMIC_ACQUIRE) == 0)
        return true;
    critical_exit();
    return false;
}

/* 释放自旋锁 */
void spinlock_release(SpinLock *lk) {
    ASSERT(lk->locked != 0);
    __atomic_store_n(&lk->locked, 0, __ATOMIC_RELEASE);
    critical_exit();
}

