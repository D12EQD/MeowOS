#pragma once

struct sbiret {
    i32 error;
    i32 value;
};


struct sbiret sbi_call(i64 arg0, i64 arg1, i64 arg2, i64 arg3, i64 arg4, i64 arg5,  i64 fid, i64 eid);


#define putchar(ch) sbi_call(((char)(ch)), 0, 0, 0, 0, 0, 0, 1) /* TODO : maybe error but i do not care*/



