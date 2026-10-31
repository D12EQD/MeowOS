.section .text
.globl _start
_start:
    la a0, msg
    li a1, 16
    li a7, 1
_loop:
    j _loop

.section .rodata
msg:
    .asciz "hello from user\n"
