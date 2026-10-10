#include "lib/def.h"
#include "arch/sbi.h"

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid, long eid) {
    register i8 a0 __asm__("a0") = arg0;
    register i8 a1 __asm__("a1") = arg1;
    register i8 a2 __asm__("a2") = arg2;
    register i8 a3 __asm__("a3") = arg3;
    register i8 a4 __asm__("a4") = arg4;
    register i8 a5 __asm__("a5") = arg5;
    register i8 a6 __asm__("a6") = fid;
    register i8 a7 __asm__("a7") = eid;

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
