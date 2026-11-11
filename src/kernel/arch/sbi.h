#pragma once
#include "kernel/lib/def.h"

struct sbiret {
    i64 value;
    i64 error;
};


struct sbiret sbi_call(i64 arg0, i64 arg1, i64 arg2, i64 arg3, i64 arg4, i64 arg5, i64 fid, i64 eid);

#define sbi_console_putchar(ch) sbi_call(((char)(ch)), 0, 0, 0, 0, 0, 0, 1)
#define sbi_console_getchar()   sbi_call(0, 0, 0, 0, 0, 0, 0, 0x02)
#define sbi_set_timer(time)     sbi_call((i64)time, 0, 0, 0, 0, 0, 0, 0x54494D45)



