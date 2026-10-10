#pragma once
#include "lib/printf.h"
#include "lib/def.h"
#define ASSERT(cond) if (!(cond)) { PANIC("Assert Error"); }

/* funcion area */
struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid, long eid);
void kernel_entry(void);
