#include "kernel/arch/console.h"
#include "kernel/arch/sbi.h"

int console_chan;

void puts(char *s){
    int idx; char a;
    for (idx = 0, a = s[0]; a; idx ++, a = s[idx]){
        sbi_console_putchar(a);
    }
}

void puts_n(char *s, u64 count){
    for (u64 i = 0; i < count; i ++){
        sbi_console_putchar(s[i]);
    }
}

