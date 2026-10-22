#include "kernel/arch/timer.h"
#include "kernel/arch/def.h"
#include "kernel/arch/sbi.h"
#include "kernel/start.h"
#include "kernel/lib/printk.h"

u64 base_time = 0; // 1s
void timer_interrupt(){
    u64 time = READ_CSR(CSR_TIME);
    ASSERT(sbi_set_timer(time + base_time / 10).error == 0); // 0.1 s
}

u64 timer_get_time(){
    u64 time = READ_CSR(CSR_TIME);
    return time;
}

void timer_init(BootInfo* boot_info){
    base_time = boot_info->timebase_frequency;
    u64 time = READ_CSR(CSR_TIME);
    ASSERT(sbi_set_timer(time + base_time / 10).error == 0); // 0.1 s
    SET_CSR(CSR_SIE, 1 << 5);
}

