.section .text
.globl _start
_start:
    la t0, msg
    j _start

.section .rodata
msg:
    .asciz "hello from user\n"

