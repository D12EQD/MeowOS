set architecture riscv:rv64
target remote :1234
layout src
br src/kernel/proc/process.c:195
