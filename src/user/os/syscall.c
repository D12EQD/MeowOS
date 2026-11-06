#include "user/def.h"
#include "user/os/syscall.h"

u64 syscall(u64 sysnumber, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    register u64 a0 __asm__("a0") = arg0;
    register u64 a1 __asm__("a1") = arg1;
    register u64 a2 __asm__("a2") = arg2;
    register u64 a3 __asm__("a3") = arg3;
    register u64 a4 __asm__("a4") = arg4;
    register u64 a5 __asm__("a5") = arg5;
    register u64 a7 __asm__("a7") = sysnumber;   // syscall 号放 a7

    __asm__ volatile (
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a7)
        : "memory"
    );

    return a0;
}

