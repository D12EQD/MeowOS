#pragma once 
#include "kernel/start.h"

#include "kernel/arch/def.h"
#include "kernel/lib/def.h"

void timer_init(BootInfo* boot_info);
void timer_interrupt();
u64 timer_get_time();
