#include "kernel/lib/def.h"
#include "kernel/arch/sbi.h"

struct sbiret sbi_call(
    i64 arg0, i64 arg1, i64 arg2, 
    i64 arg3, i64 arg4, i64 arg5, 
    i64 fid, i64 eid
) {
    register i64 a0 __asm__("a0") = arg0;
    register i64 a1 __asm__("a1") = arg1;
    register i64 a2 __asm__("a2") = arg2;
    register i64 a3 __asm__("a3") = arg3;
    register i64 a4 __asm__("a4") = arg4;
    register i64 a5 __asm__("a5") = arg5;
    register i64 a6 __asm__("a6") = fid;
    register i64 a7 __asm__("a7") = eid;

    _ASM("ecall"
        : "=r"(a0), "=r"(a1)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
        : "memory"
    );
    return (struct sbiret){
        .error = a0, 
        .value = a1
    };
}



