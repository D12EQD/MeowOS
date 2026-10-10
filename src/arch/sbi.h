#pragma once

struct sbiret {
    i32 error;
    i32 value;
};

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid, long eid);

#define putchar(ch) sbi_call(((char)(ch)), 0, 0, 0, 0, 0, 0, 1) /* TODO : maybe error but i do not care*/



