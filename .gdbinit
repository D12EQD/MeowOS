set architecture riscv:rv64
target remote :1234
layout reg
layout asm
br user_trap_entry
br trap_ret
br user_trap_ret 
