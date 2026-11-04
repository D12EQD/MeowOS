#include "kernel/start.h"

#include "kernel/arch/timer.h"
#include "kernel/arch/def.h"
#include "kernel/arch/sbi.h"

#include "kernel/lib/printk.h"
#include "kernel/proc/process.h"

i64 base_time = -1; // 1s
void timer_interrupt(){
    u64 time = READ_CSR(CSR_TIME);
    ASSERT(sbi_set_timer(time + base_time).error == 0); // 1 s
    yield();
}

u64 timer_get_time(){
    u64 time = READ_CSR(CSR_TIME);
    return time;
}

void timer_init(BootInfo* boot_info){
    base_time = boot_info->timebase_frequency; // set base_time

    ASSERT(base_time >= 0);

    u64 time = READ_CSR(CSR_TIME);
    ASSERT(sbi_set_timer(time + base_time).error == 0); // 1 s
    SET_CSR(CSR_SIE, 1 << 5);
}

